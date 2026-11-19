# Project135 — Core API и контракты модулей

Версия документа: 0.1  
Статус: базовый контракт для P135.001  
Язык реализации: C++20

## 1. Архитектурные правила

1. `Project135Core` не зависит от Qt, OCCT, SQLite, Gmsh или Elmer.
2. Все физические величины внутри ядра представлены в единицах СИ.
3. Типы величин должны предотвращать смешение несовместимых величин (например, RMS и peak, линейных и фазных напряжений).
4. Расчётный модуль не читает UI, файлы и БД напрямую. Зависимости передаются через интерфейсы/контекст.
5. Каждый расчёт получает неизменяемый снимок входной модели и версии используемых алгоритмов/данных.
6. Отсутствующие исходные данные не заменяются молча типовыми значениями. Непроверяемые требования получают `NOT_VERIFIED`.
7. Ошибки физической модели, некорректные входные данные, отсутствие свойств материала, сходимость и отмена — разные типы результата.
8. CAD — производное представление инженерной модели. Ручная правка геометрии не меняет электрические параметры автоматически.

## 2. Базовые типы

Рекомендуемый namespace: `p135`.

### 2.1 Идентификаторы и версии

```cpp
namespace p135 {
using ProjectId = Uuid;
using ProductId = Uuid;
using ModelId = Uuid;
using CalculationId = Uuid;
using MaterialId = Uuid;
using ComponentId = Uuid;
using RevisionId = Uuid;

struct SemanticVersion {
    std::uint16_t major{}, minor{}, patch{};
    std::string build;
};

struct Provenance {
    SemanticVersion applicationVersion;
    SemanticVersion algorithmVersion;
    SemanticVersion materialDatabaseVersion;
    std::string standardsEditionSet;
    std::string inputSnapshotSha256;
    DateTimeUtc calculatedAt;
};
}
```

`Uuid`, `DateTimeUtc` и `Result` — собственные типы ядра либо небольшие типы-обёртки. Не использовать `QString`, `QDateTime`, `QVariant` в публичных интерфейсах Core.

### 2.2 Размерные величины

Внутренние значения: V, A, W, var, VA, ohm, H, F, Hz, s, m, kg, K, °C только на границе ввода/вывода. Углы — rad.

Не передавать в расчёт «голые» числа с неявными единицами. В API использовать типы величин или явно именованные поля с суффиксом единицы. Минимальный набор типизированных величин:

- Voltage, Current, Resistance, Conductance
- Power, ReactivePower, ApparentPower
- Inductance, Capacitance, Frequency, AngularFrequency
- Length, Area, Volume, Mass
- TemperatureKelvin, TemperatureCelsius, TemperatureDifference
- MagneticFlux, FluxDensity, MagneticFieldStrength
- Time, Angle, Efficiency, Dimensionless

Преобразования единиц выполняются на границе UI/импорта. Температурные разности нельзя конвертировать как абсолютную температуру.

## 3. Result, ошибки и диагностика

```cpp
enum class ErrorCode {
    InvalidInput,
    MissingRequiredData,
    MissingMaterialProperty,
    UnsupportedConfiguration,
    ModelOutOfDomain,
    SingularSystem,
    NonConvergence,
    SolverFailure,
    Cancelled,
    IoFailure,
    InternalError
};

struct Diagnostic {
    Severity severity;             // info, warning, error
    std::string code;              // стабильный машинный код
    std::string message;
    std::optional<std::string> fieldPath;
    std::optional<double> measuredValueSI;
    std::optional<double> limitValueSI;
};

struct Error {
    ErrorCode code;
    std::string message;
    std::vector<Diagnostic> diagnostics;
};

template<class T>
using Result = Expected<T, Error>;
```

Предупреждение не должно превращаться в ошибку автоматически. Однако расчёт не может быть помечен как успешный, если обязательная физическая модель не рассчитана или система не сошлась.

Каждый модуль возвращает диагностические сообщения с устойчивым `code`, чтобы UI, отчёты и регрессионные тесты не зависели от текста сообщения.

## 4. Контекст расчёта

```cpp
struct CalculationContext {
    CancellationToken cancellation;
    IProgressSink* progress;            // optional, non-owning
    ILogger* logger;                    // optional, non-owning
    const IMaterialRepository& materials;
    const IStandardsRepository& standards;
    Provenance provenance;
};
```

Контекст создаётся приложением на один запуск расчёта. Модули не владеют переданными указателями. Долгие итерационные алгоритмы обязаны периодически проверять cancellation token и публиковать прогресс, если sink предоставлен.

## 5. Электрическая модель

### 5.1 Топология

```cpp
enum class Phase { A, B, C, N, PE };
enum class Connection { Wye, Delta, SinglePhase };
enum class ElementKind {
    Resistor, Inductor, Capacitor, MutualInductance,
    VoltageSource, CurrentSource, Switch, MotorEquivalent,
    CableEquivalent, TransformerEquivalent
};

struct Node { NodeId id; std::string name; };
struct TwoTerminalElement {
    ComponentId id;
    NodeId positive;
    NodeId negative;
    ElementKind kind;
    ComponentParameters parameters;
};
struct CircuitTopology {
    std::vector<Node> nodes;
    std::vector<TwoTerminalElement> elements;
    NodeId referenceNode;
};
```

Для взаимной индуктивности задаётся отдельная связь между катушками с коэффициентом связи/взаимной индуктивностью и однозначной конвенцией точек. Трёхфазные соединения и нейтраль должны быть явными элементами/узлами, а не скрытыми допущениями решателя.

### 5.2 Исследования и результаты

```cpp
enum class AnalysisKind { DcOperatingPoint, AcSteadyState, Transient };
struct SolverOptions {
    double relativeTolerance;
    double absoluteTolerance;
    std::uint32_t maxIterations;
};
struct ElectricalStudy {
    CircuitTopology topology;
    AnalysisKind analysis;
    SolverOptions solver;
    std::vector<Frequency> frequencies;
    std::optional<Time> stopTime;
    std::optional<Time> timeStep;
};
struct ElectricalResult {
    std::vector<NodeVoltage> nodeVoltages;
    std::vector<BranchCurrent> branchCurrents;
    std::vector<ComponentPower> componentPowers;
    PowerBalance powerBalance;
    SolverDiagnostics convergence;
};
```

Для AC используются комплексные фазоры. Результат обязан явно указывать RMS/peak convention. Проверяется баланс активной мощности с допуском, заданным настройками решателя.

### 5.3 Контракт

```cpp
struct IElectricalSolver {
    virtual ~IElectricalSolver() = default;
    virtual Result<ElectricalResult> solve(
        const ElectricalStudy&, const CalculationContext&) const = 0;
};
```

## 6. Гармоники и частотные характеристики

Входной спектр хранит комплексную составляющую каждой гармоники (амплитуда и фаза), базовую частоту и соглашение RMS/peak. Не суммировать амплитуды арифметически: суммарный RMS для ортогональных гармоник вычисляется как `sqrt(sum(I_h^2))`.

```cpp
struct HarmonicComponent {
    std::uint32_t order;
    ComplexRms current;
    ComplexRms voltage;
};
struct HarmonicSpectrum {
    Frequency fundamental;
    std::vector<HarmonicComponent> components;
};
struct HarmonicStudy {
    CircuitTopology network;
    HarmonicSpectrum sourceSpectrum;
    FrequencyRange sweep;
    std::vector<ComponentFrequencyModel> frequencyModels;
};
struct HarmonicResult {
    std::vector<HarmonicOperatingPoint> byHarmonic;
    FrequencyResponse response;
    ResonanceReport resonances;
    DistortionMetrics distortion;
    LossBreakdown losses;
};
struct IHarmonicsSolver {
    virtual ~IHarmonicsSolver() = default;
    virtual Result<HarmonicResult> solve(
        const HarmonicStudy&, const CalculationContext&) const = 0;
};
```

Модели конденсаторов и проводников должны поддерживать частотную зависимость. Для конденсатора учитываются как минимум C, ESR, ESL, диэлектрические потери, допуски и температурная зависимость, если соответствующие данные доступны.

## 7. Магнитная модель

```cpp
enum class CoreGeometry {
    ThreeLimbThreePhase, ThreeSinglePhase,
    EI, UI, Toroidal, Custom
};
struct MagneticCore {
    CoreGeometry geometry;
    MaterialId material;
    CoreDimensions dimensions;
    AirGapModel airGap;
    LaminationModel lamination;
};
struct MagneticStudy {
    MagneticCore core;
    std::vector<CoilExcitation> windings;
    MagneticSolverOptions solver;
};
struct MagneticResult {
    FluxDistribution flux;
    FieldDistribution field;
    std::vector<CoreLossResult> losses;
    SaturationReport saturation;
    LeakageInductance leakage;
};
struct IMagneticSolver {
    virtual ~IMagneticSolver() = default;
    virtual Result<MagneticResult> solve(
        const MagneticStudy&, const CalculationContext&) const = 0;
};
```

Материалы задаются кривыми B-H и данными потерь в зависимости от B, f и T. Для шихтованной стали требуется модель листов/анизотропии и межлистовых потерь; нельзя автоматически считать пакет пластин сплошным массивом стали. Воздушный зазор учитывает fringing.

## 8. Обмотки

```cpp
enum class ConductorShape { Round, Rectangular, Foil, Busbar, Litz };
struct Conductor {
    MaterialId material;
    ConductorShape shape;
    ConductorDimensions dimensions;
    std::uint32_t parallelCount;
};
struct Winding {
    ComponentId id;
    std::uint32_t turns;
    Conductor conductor;
    WindingLayout layout;
    InsulationSystemId insulation;
    WindingPlacement placement;
};
struct WindingResult {
    Resistance dcResistance;
    FrequencyDependentResistance acResistance;
    CurrentDensity currentDensity;
    CopperLoss loss;
    WindingDimensions dimensions;
};
struct IWindingCalculator {
    virtual ~IWindingCalculator() = default;
    virtual Result<WindingResult> calculate(
        const Winding&, const WindingEnvironment&,
        const CalculationContext&) const = 0;
};
```

Расчёт AC-сопротивления должен учитывать skin/proximity effect, параллельные жилы и распределение тока. Геометрия проводника, изоляционные промежутки и коэффициент заполнения должны быть согласованы с CAD-моделью.

## 9. Тепловая модель

```cpp
enum class CoolingMode { Natural, ForcedAir, Liquid };
struct ThermalStudy {
    ThermalGeometry geometry;
    ThermalBoundaryConditions boundaries;
    std::vector<HeatSource> losses;
    CoolingMode cooling;
};
struct ThermalResult {
    TemperatureField temperatures;
    TemperatureKelvin hotspot;
    ThermalResistanceNetwork network;
    ThermalDiagnostics diagnostics;
};
struct IThermalSolver {
    virtual ~IThermalSolver() = default;
    virtual Result<ThermalResult> solve(
        const ThermalStudy&, const CalculationContext&) const = 0;
};
```

Первый этап — тепловая сеть с проводимостью, конвекцией и излучением. Тепловой FEM и связанный EM-thermal — последующие этапы. Все потери должны поступать из электрических/магнитных расчётов с указанием источника.

## 10. FEM

Общий FEM-решатель не разрабатывается с нуля. Ядро определяет нейтральный контракт; адаптеры интегрируют Gmsh/GetDP и/или Elmer.

```cpp
enum class FemPhysics { Magnetostatic, HarmonicMagnetic, EddyCurrent, Thermal };
struct FemStudy {
    FemPhysics physics;
    GeometryModel geometry;
    MaterialAssignments materials;
    BoundaryConditions boundaries;
    MeshOptions mesh;
    FemSolverOptions solver;
};
struct FemResult {
    FieldSolution fields;
    LossBreakdown losses;
    MeshStatistics mesh;
    SolverDiagnostics convergence;
};
struct IFemSolver {
    virtual ~IFemSolver() = default;
    virtual Result<FemResult> solve(
        const FemStudy&, const CalculationContext&) const = 0;
};
```

Каждый FEM-результат содержит версию backend, сетку/статистику сетки, граничные условия, критерий сходимости и диагностику. Результаты сравниваются с аналитическими эталонами и тестовыми задачами.

## 11. CAD

```cpp
struct CadOptions {
    Length linearTolerance;
    Angle angularTolerance;
    bool generateAssembly;
    bool generateDrawings;
};
struct CadArtifact {
    GeometryModelId sourceGeometry;
    std::vector<CadFile> files;
    CadValidationReport validation;
};
struct ICadGenerator {
    virtual ~ICadGenerator() = default;
    virtual Result<CadArtifact> generate(
        const GeometryModel&, const CadOptions&,
        const CalculationContext&) const = 0;
};
```

OCCT — реализация адаптера, а не тип публичного Core API. Экспорт STEP/IGES, DXF и PDF выполняется отдельными экспортёрами. Генерация обязана сохранять связь артефакта с версией исходной инженерной модели.

## 12. Нормативная проверка

```cpp
enum class ComplianceStatus {
    Pass, Fail, Warning, NotApplicable, NotVerified
};
struct ComplianceCheck {
    std::string standardId;
    std::string edition;
    std::string clause;
    std::string method;
    std::vector<std::string> requiredInputs;
};
struct ComplianceItemResult {
    ComplianceCheck check;
    ComplianceStatus status;
    std::vector<Diagnostic> evidence;
    std::string limitations;
};
struct ComplianceReport {
    std::vector<ComplianceItemResult> items;
};
struct IStandardsChecker {
    virtual ~IStandardsChecker() = default;
    virtual Result<ComplianceReport> check(
        const ComplianceStudy&, const CalculationContext&) const = 0;
};
```

Правила хранятся как данные отдельно от решателей. Для каждого результата фиксируются стандарт, редакция, пункт, применимость, метод, входы, ограничения и доказательства. Предварительный список стандартов в сценарии проекта не означает автоматическую применимость или сертификацию.

## 13. Оптимизация

```cpp
struct Objective {
    ObjectiveKind kind; // cost, mass, dimensions, losses, hotspot
    ObjectiveDirection direction;
};
struct OptimizationProblem {
    DesignVariables variables;
    std::vector<Objective> objectives;
    std::vector<EngineeringConstraint> constraints;
    std::uint32_t evaluationBudget;
};
struct OptimizationResult {
    std::vector<DesignCandidate> paretoFront;
    OptimizationDiagnostics diagnostics;
};
struct IOptimizer {
    virtual ~IOptimizer() = default;
    virtual Result<OptimizationResult> optimize(
        const OptimizationProblem&, const CalculationContext&) const = 0;
};
```

Оптимизатор не содержит собственных упрощённых физических формул: он вызывает те же расчётные контракты, что и обычный расчёт изделия.

## 14. Хранение и воспроизводимость

Расчёт сохраняется как неизменяемая запись:
- идентификатор и ревизия изделия;
- полный snapshot входной модели;
- версии приложения, алгоритмов и material database;
- редакции нормативных документов;
- параметры решателя и FEM backend;
- выходные данные, предупреждения и ошибки;
- дата/время UTC;
- хэш snapshot.

Изменение модели создаёт новую ревизию. Старый результат не перезаписывается. Сравнение ревизий показывает изменения входов и результатов.

## 15. Сборки CMake

Целевые библиотеки:
- `Project135Domain` — сущности и инженерные типы;
- `Project135Core` — общие контракты, Result, diagnostics, context;
- `Project135Circuit`, `Project135Harmonics`, `Project135Magnetic`, `Project135Winding`, `Project135Thermal`;
- `Project135FemApi` — интерфейс FEM без backend-зависимостей;
- `Project135CadApi` — интерфейс CAD без OCCT-зависимостей;
- `Project135Standards`, `Project135Optimization`;
- `Project135Infrastructure` — SQLite, файловая система, логирование;
- `Project135Desktop` — Qt UI.

Зависимости направлены внутрь: Desktop/Infrastructure адаптируют внешние технологии к контрактам ядра. Domain/Core не зависят от UI и инфраструктуры.

## 16. Критерии готовности API

- Core компилируется без Qt и OCCT.
- Все интерфейсы принимают const-входы и возвращают Result.
- Отмена и диагностика доступны каждому длительному решателю.
- Единицы измерения и RMS/peak convention однозначны.
- Нет PASS при отсутствии обязательных данных.
- Один и тот же snapshot с теми же версиями воспроизводит результат в пределах заданных численных допусков.
- Shunt filter и VFD output filter используют общие Circuit/Harmonics/Magnetic/Winding/Thermal контракты, но имеют отдельные product-level модели и профили требований.
