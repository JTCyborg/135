# Project135 — сквозной сценарий реализации расчёта, графиков и 3D CAD

Статус: рабочий план и обязательный чек-лист реализации
Ветка: main
Эталонный сценарий: TEST-01 — трёхфазный шунтирующий однонастроенный LC-фильтр с трёхстержневым реактором.

## 1. Цель

Довести приложение от исходных данных до согласованной инженерной ревизии, расчётных результатов, графиков и параметрической 3D-сборки. Не создавать параллельные модели: использовать существующие domain/application/core/CAD модули и единый источник инженерных данных.

## 2. Зафиксированные решения

1. Электрическая модель фильтра и её immutable snapshot — источник возбуждения для расчёта реактора.
2. Физические размеры магнитопровода и обмоток задаются явно. Нельзя восстанавливать уникальную CAD-геометрию только по эффективным Ae, Aw, le.
3. Аналитический first-pass sizing, физический магнитный расчёт, тепловая оценка и CAD-геометрия — разные артефакты с явными статусами и источниками.
4. Предварительная CAD-сборка разрешена при наличии согласованного набора физических размеров; маркировка PRELIMINARY / NOT_VERIFIED. Построение CAD не означает электрическую, магнитную, тепловую, производственную или нормативную проверку.
5. Отсутствующие данные не заменяются типовыми числами автоматически. Статусы: NOT_PROVIDED, NOT_APPLICABLE, NOT_VERIFIED, READY, FAILED, PRELIMINARY.
6. Изменение входа инвалидирует зависимые результаты; принятые старые ревизии сохраняются.
7. Внутренние величины — SI; единицы UI и преобразования задаются явно.
8. В TEST-01 использовать только явно введённые параметры. Bmax, J, Ku, Ks и Aw/Ae — проектные критерии, а не подтверждение реальных свойств материала/конструкции.

## 3. Последовательность реализации и чек-лист

### A. Аудит и фиксация архитектуры
- [x] Проверены ReactorDesignInputs, dependency graph/execution API, физическая геометрия и OCCT assembly builder.
- [x] Подтверждено: main_window.cpp пока не вызывает executeReactorDesignPlan() и не передаёт результат в buildThreeLimbReactorPresentation().
- [x] Подтверждено: CAD viewport и генераторы OCCT существуют; пустое окно связано с отсутствием физической геометрии и UI-интеграции, а не отсутствием CAD-движка.
- [x] Зафиксирован этот план.

### B. Единая модель и входные данные Construction

Текущий прогресс (коммиты 7581fbb, 5ea8a5c, f7be5fd, 74d963e, d434865, 429495a, c8483be, 87a95bd, c113adc, c15294f, 46a3216, 15fe373, a4deae9, 9bb67f7, 500190d, 4891812, fbe20f3, 4d85398, 8533be7, 111b01c, 93bb5c7, 2db2096, 76df6ef, 4f3d421, aa3ffef, ff79705, c6d3f51):
- [x] В application-модели заведены типизированные поля физических размеров сердечника, окон, ярма, каркасов и winding-pack envelopes (SI).
- [x] В модели разделены число воздушных зазоров на каждом стержне, суммарная длина и отдельные записи положения/длины каждого зазора.
- [x] Добавлены записи охлаждающих каналов с положением, толщиной, ревизией материала распорки и направлением потока.
- [x] Добавлены отдельные поля bare/insulated diameter и conductor insulation build для круглого проводника.
- [x] Добавлена отдельная insulation thermal-class reference с источником, стандартом, температурой класса, safety margin и maximum design winding temperature.
- [x] Все новые физические поля включены в validate() и requiresRecalculation(); добавлены regression assertions в application input tests.
- [x] Construction UI и collectReactorDesignInputs() связаны с явными ширинами/глубинами трёх стержней, размерами двух окон, толщинами/глубинами четырёх сегментов ярма.
- [x] Construction UI сохраняет количество дискретных зазоров и суммарную длину зазоров отдельно для каждого стержня.
- [x] Добавлены три явных слота позиции/длины зазора на каждый стержень; подтверждение Construction сверяет число записей и сумму длин с агрегированными значениями.
- [x] Добавлены UI-поля межосевых расстояний A-B/B-C/A-C и общих высоты/ширины сердечника.
- [x] Cooling duct UI: count and up to three duct records with position, thickness, exact spacer material revision ID and airflow direction; confirmation rejects partial records/count mismatch.
- [x] Для проводника добавлены отдельные bare/insulated diameter controls круглой формы и conductor insulation build для прямоугольной/фольговой формы; неподходящие контролы скрываются, а collectReactorDesignInputs() захватывает только применимую геометрию.
- [x] Добавлены три записи индивидуальных воздушных зазоров на каждый стержень; подтверждение проверяет соответствие количества и суммы длин, а dependency invalidation учитывает новые геометрические поля.
- [x] Добавлены межосевые расстояния A-B/B-C/A-C, общие габариты сердечника, физические размеры каркаса и winding-pack для фаз A/B/C.
- [x] Thermal class reference включает явно вводимые стандарт, опорную температуру и safety margin; нормативная температура автоматически не подставляется.
- [x] Исправлены value equality для новых записей и добавлен явный include для std::array; проверена актуальная версия заголовка через GitHub.
- [x] Добавлены thermal class, стандарт, safety margin и отдельный maximum design winding temperature; thermal class остаётся справочной категорией.
- [x] Добавлен строгий application adapter ReactorDesignInputs → ReactorPhysicalGeometry: только явные размеры, источник и ревизия; domain validation выполняется до CAD. Неизвестные размеры не подставляются.
- [x] CAD вкладка подключена к adapter, buildThreeLimbReactorPresentation() и collectAssemblyPresentationDisplayItems(); кнопка строит PRELIMINARY / NOT_VERIFIED preview после подтверждения Construction.
- [x] Любое изменение Construction инвалидирует CAD preview: viewport очищается, дерево получает STALE, генерация снова разрешается только после повторного подтверждения.
- [x] Semantic component tree строится из AssemblyPresentationNode; visibility checkboxes и transparency control управляют тем же presentation model и обновляют viewport.
- [x] Добавлен STEP export полного assembly B-Rep; перед записью повторно проверяются semantic tree и B-Rep (это геометрическая проверка, не инженерная сертификация), ошибки Transfer/Write возвращаются как Result.
- [x] CAD regression test проверяет создание STEP-файла, повторное чтение через STEPControl_Reader, B-Rep validity и наличие solids.
- [x] Пользователь подтвердил Windows/MSVC Debug E2E: синтетическая TEST-01 геометрия построена через UI; STEP export → OCCT import → B-Rep validity → 13 solids прошли. Это подтверждает геометрический regression fixture, не производственную конструкцию.

- [ ] Сопоставить каждый UI control с полем типизированной ReactorDesignInputs; убрать поля, существующие только в UI/JSON.
- [x] UI содержит явные физические размеры стержней, глубины пакета, окон, сегментов ярма, межосевые расстояния и общие габариты.
- [x] Для каждого стержня доступны три записи положения/длины; число и суммарная длина задаются отдельно и проверяются при подтверждении.
- [x] Добавлены размеры внутреннего окна, осевой длины и толщины стенки каркаса, а также радиальной сборки, осевой высоты и торцевого зазора winding-pack для фаз A/B/C. Детальная укладка проводников остаётся отдельной задачей.
- [x] Добавлены размеры фланцев каркасов по каждой фазе: толщина и внешние ширина/высота; параметры включены в validate/requiresRecalculation и application regression tests.
- [x] Добавлены явные описание конструкции пакета магнитопровода и число секций обмотки.
- [x] Добавлены отдельные поля источника и ревизии физических размеров; они передаются в типизированную модель, проверяются как согласованная пара, входят в invalidation и regression tests. JSON Debug snapshot собирает оба поля автоматически.
- [x] Добавлены восемь независимых селекторов Butt/Step-Lap и редактор packet schedule: joint, stack-depth offset/depth, overlaps into limb/yoke, alternating direction; расписание сохраняется в Debug snapshot и project settings, проверяется и участвует в dependency invalidation.
- [ ] Реализовать физически корректную interleaved Step-Lap B-Rep топологию. Domain packet validation и application adapter готовы, но OCCT builder намеренно блокирует Step-Lap, пока не определена и не реализована безпересекающаяся геометрия пакетов.
- [x] Добавлены Construction controls для числа и трёх охлаждающих каналов; все четыре параметра каждой записи передаются в ReactorDesignInputs и включены в snapshot/invalidation.
- [ ] Для круглого/прямоугольного/фольгового проводника включать только применимые размеры; неприменимые поля — NOT_APPLICABLE, не ноль.
- [x] Добавлен явный bobbin winding clearance per phase, включён в snapshot, validation и dependency invalidation.
- [x] Thermal class reference отделён от maximum design winding temperature; класс, стандарт и safety margin сохраняются отдельно.
- [ ] Проверить наследование Project → Filter definition → Construction и snapshot lineage.

### C. Реальный execution pipeline

Промежуточная реализация:
- Добавлен typed adapter `makeMagneticOperatingPointStageArtifact()` поверх существующего `analyzePhysicalFilterReactorMagneticCycle()`: он принимает фактический результат нелинейного трёхстержневого магнитного решателя, требует совпадающий filter branch, явные source/revision физических размеров и ревизию B-H данных, а также отклоняет неконвергентный или вышедший за измеренную B-H кривую operating point.
- Добавлены regression tests для успешной сериализации, отсутствующей ревизии геометрии, несовпадающего branch, отсутствия сходимости и B-H extrapolation.
- Это пока adapter, а не подключённая desktop pipeline stage: `magnetic.operating_point` не должна считаться VALID до интеграции с реальным desktop calculation result и проверки upstream revision lineage.
- Adapter дополнительно допускает физически корректный нулевой поток в отдельной ветви; отрицательные и неfinite peak |B| отклоняются. Provenance-поля ограничены одной строкой, чтобы сериализованный key/value artifact нельзя было расширить внедрёнными строками.
- Проверка desktop integration выявила, почему нельзя напрямую передать существующий `threeLimbMagneticBase` в новый adapter: текущий `MainWindow::calculateFilter()` строит его из legacy полей Ae/le/air gap/yoke и пользовательской B-H таблицы, а не из подтверждённого `ReactorDesignInputs → ReactorPhysicalGeometry`. Ветка частотного sweep также является source-limited single-frequency diagnostic, не multi-harmonic harmonic operating point. Автоматически переиспользовать её как физическую ревизию запрещено.
- Следующая обязательная работа перед desktop stage: собрать immutable stage context из подтверждённого Construction snapshot, точной ревизии CoreMaterial и уже рассчитанного harmonic branch result; затем вызвать `analyzePhysicalFilterReactorMagneticCycle()` ровно один раз и адаптировать тот же результат. Если snapshot/material/turns/harmonic spectrum отсутствуют, stage должна остаться NOT_VERIFIED с конкретной причиной.


- [x] Desktop сохраняет типизированный `ReactorDesignInputs` как принятую Construction-ревизию после подтверждения; CAD использует именно этот захваченный набор, а не повторно читает изменяемые UI-контролы. Любое изменение Construction или загрузка другого проекта сбрасывает принятую ревизию. Магнитный stage bridge ещё не подключён.
- [ ] Построить input fingerprint и dependency plan из изменённых входов.
- [ ] Подключить реальные stage adapters к существующим магнитным, winding, loss и thermal use cases.
- [ ] Не отмечать stage Valid, если он вернул placeholder или отсутствуют обязательные источники.
- [ ] Сделать выполнение транзакционным: ошибка stage не перезаписывает принятую ревизию.
- [ ] Выводить состояние каждого этапа, причину блокировки и отсутствующие входы.
- [ ] Сохранять immutable result snapshot, ревизии материалов, версии алгоритмов и исходный filter snapshot.

### D. Расчётные результаты и графики
- [ ] Разделить графики по физическим источникам данных; исключить повторное использование одной кривой для разных величин.
- [ ] Для каждого графика отображать READY / WAITING_FOR_INPUT / NOT_VERIFIED / FAILED и причину.
- [ ] Частотная характеристика и bus impedance: использовать circuit/harmonic frequency sweep.
- [ ] Токи/напряжения: строить по рассчитанным комплексным гармоникам, различать RMS и peak.
- [ ] Магнитные графики: строить только из магнитного решения и реальных B-H данных.
- [ ] Потери и температурные графики: строить только после получения потерь и тепловых параметров.
- [ ] Проверять единицы, конечность значений, диапазоны и regression tests каждого графика.
- [ ] Сохранить диагностический график идеального источника, не выдавая его за эксплуатационный стресс.

### E. Параметрическая 3D CAD
- [x] Сформировать ReactorPhysicalGeometry из согласованных физических размеров через строгий adapter; отсутствующие данные блокируют генерацию.
- [x] Валидировать окна, зазоры, стыки, габариты каркасов и winding envelopes до OCCT через domain validation.
- [x] Подключить buildThreeLimbReactorPresentation() и collectAssemblyPresentationDisplayItems() к CAD вкладке.
- [x] Отображать семь сегментов сердечника, три каркаса и три winding-pack envelopes; статус PRELIMINARY / NOT_VERIFIED и ограничения детализации показаны в UI.
- [x] Показать semantic component tree, свойства выбранного компонента и статусы; visibility/transparency управляются по стабильным component IDs. До генерации показывается только placeholder, без вымышленных компонентов.
- [ ] Добавить материал-ориентированное оформление 3D viewport: цвет и параметры визуального материала привязывать к подтверждённым material revision IDs (core steel, conductor copper, insulation, bobbin); не выводить физический материал из одного только component ID. Для каждой B-Rep поверхности использовать независимое освещение/Phong shading по нормали поверхности с контролируемыми diffuse/specular/roughness параметрами, сохраняя общий базовый цвет material role; selection highlight и transparency не должны уничтожать исходную палитру. Если material revision отсутствует — нейтральный NOT_VERIFIED стиль и явная легенда, без выдуманного назначения.
- [x] Добавить экспорт STEP после валидации B-Rep.
- [ ] Не называть упрощённые оболочки производственной геометрией.

### F. Сквозная проверка и выпуск

Текущая реализация автоматизации:
- [x] Выделена библиотека `Project135DesktopUi`, чтобы тестировать тот же `MainWindow`, который используется desktop-приложением.
- [x] Добавлен Qt Test target `Project135DesktopWorkflowE2ETests` с запуском в offscreen-режиме и отдельной копией SQLite migrations.
- [x] Добавлены проверки порядка семи вкладок, явного переключения на каждую вкладку, наличия основных workflow actions и стабильных objectName у редактируемых контролов Project / Filter definition / Construction.
- [x] Добавлен UI-путь подтверждений Project → Filter definition → Construction и запуска расчёта с подтверждением Calculation & design.
- [x] Проверка 2D drawing action переведена с XFAIL на PASS: кнопка `generateCadDrawings` присутствует и до создания актуальной CAD-геометрии остаётся отключённой. Windows/MSVC Debug подтверждён пользователем на commit `2d7e4bd`.
- [ ] Заполнить все поля всех вкладок детерминированным TEST-01 UI fixture; для каждого snapshot сверять полный набор control IDs и сохранённых значений. Новый тест покрывает CAD-обязательные Construction поля и часть электрических/тепловых полей; полный перебор всех полей вкладок и snapshot round-trip остаётся TODO.
- [ ] Проверить численные значения TEST-01 через UI, включая C, L, R и токи, а не только факт появления строк результатов.
- [x] Добавить в desktop E2E численные regression assertions для фазного напряжения, C, L и R TEST-01 с допусками; пользователь подтвердил успешный Windows/MSVC Debug CTest 1/1 (16.71 s).
- [x] Подготовить и прогнать через CAD UI синтетический TEST-01 CAD regression fixture: точные seeded material revision IDs, явная геометрическая provenance, восемь Butt joints, все размеры core/bobbin/winding, OCCT assembly tree и STEP round-trip (B-Rep validity, 13 solids). Windows/MSVC Debug подтверждён пользователем на commit `2d7e4bd`; геометрия остаётся синтетической и NOT manufacturing-approved, это не реальная производственная ревизия.
- [x] Добавить и подтвердить Windows E2E negative-path: при неполных Construction inputs CAD generation отказывает, оставляет placeholder assembly tree и не разрешает STEP export.
- [x] Добавлен preliminary DXF generator из той же ReactorPhysicalGeometry revision: front elevation, plan view, side elevation, overall dimensions, SI→mm, DXF INSUNITS, source/revision provenance и PRELIMINARY / NOT VERIFIED маркировка. Step-Lap намеренно блокируется, поскольку его физическая пакетная топология ещё не поддержана.
- [x] DXF расширен явными размерными цепями по введённым данным: ширины стержней A/B/C, ширины окон AB/BC, четыре толщины ярма, глубины пакетов стержней/ярма, осевая длина и глубина каркаса фазы B, а также аннотации winding envelope. Размеры не являются допусками или manufacturing dimensions; Windows/MSVC Debug E2E подтверждён пользователем после расширения: 9 QtTest passed, 0 failed; CTest 1/1 passed.
- [x] Расширение DXF: добавлены четыре отдельных контура ярма в плановую проекцию с индивидуальными глубинами; фронтальная проекция разрывает контуры стержней/ярма по физическим air-gap intervals; боковая проекция показывает центральный стержень и четыре ярма с индивидуальными глубинами/толщинами; добавлена ведомость air-gap records. Размеры/координаты берутся из той же ReactorPhysicalGeometry, что и OCCT builder. Пользователь подтвердил Windows/MSVC Debug E2E: 9 passed, 0 failed, CTest 1/1; проверены четыре ярма и пустая ведомость зазоров.
- [x] CAD UI получил отдельную кнопку DXF export; действие доступно только после успешной генерации актуальной 3D CAD revision и инвалидируется вместе с Construction.
- [x] Выполнить Windows/MSVC E2E: DXF экспортирован через реальный UI action; файл проверен на три ортогональных вида, INSUNITS, общие габаритные размеры, winding envelope и source/revision provenance. Пользователь подтвердил PASS на commit `2d7e4bd`.
- [ ] Провести геометрический аудит DXF: сверить координаты и проекции с тем же OCCT B-Rep, проверить расположение размерных линий/текста, непересечения и корректность размерных баз для несимметричных стержней/окон/ярм.
- [ ] Добавить отдельные листы/файлы деталей сердечника, каркасов и обмоточных пакетов только в пределах реально моделируемой геометрии; не детализировать проводник или пакет Step-Lap без соответствующих физических моделей.
- [ ] Формировать BOM только из компонентной модели с подтверждёнными материалами, количеством и единицами; неизвестные массы/марки/покупные позиции оставлять NOT_PROVIDED.
- [x] Пользователь подтвердил Windows/MSVC Debug E2E для одного ненулевого зазора Limb A: DXF schedule (position 80 mm, length 2 mm), STEP round-trip и UI export; 9 passed, CTest 1/1, 14 solids.
- [x] Пользователь подтвердил Windows/MSVC Debug E2E сценария с двумя зазорами: Limb A (80/2 mm) и Top Yoke AB (40/2 mm); DXF schedule (2), STEP round-trip с 15 solids (файл 9848 entities), QtTest 9 passed / 0 failed / 0 skipped, CTest 1/1 passed (37.82 s). Это подтверждает UI/export pipeline и состав STEP, но не координатную точность DXF cut-face projections; она остаётся отдельным пунктом геометрического аудита.
- [ ] Проверить инвалидацию: геометрия → магнитные/тепловые/CAD/drawing результаты; материал → зависимые расчёты.
- [ ] Проверить Debug и Release, сохранение/загрузку проекта, immutable snapshots.
- [x] Запустить desktop UI E2E тест в Windows/MSVC Debug: пользователь подтвердил commit `2d7e4bd`, сборка target успешна, CTest 1/1 passed (36.52 s; QtTest 9 passed, 0 failed; STEP round-trip и DXF UI export проверки PASS).
- [ ] Запустить desktop UI тест в Windows/MSVC Release; фиксировать результаты, не объявлять PASS без фактического запуска.

## 4. Критерии приёмки

- Ни одно обязательное поле не остаётся немаркированно пустым.
- Каждый недоступный результат имеет статус и объяснение.
- Графики воспроизводимы из расчётных артефактов.
- CAD-сборка создаётся из явных физических размеров и проходит геометрическую валидацию.
- Инженерная ревизия, графики и CAD имеют один revision/fingerprint.
- Неполные данные не приводят к ложному PASS или вымышленной геометрии.

## 5. Текущий технический вывод

- [x] Расширен сериализуемый артефакт `magnetic.operating_point`: теперь сохраняет исходные физические размеры сердечника, соединения/Step-Lap пакеты, положения зазоров, эффективную магнитную геометрию, витки по фазам, B-H-кривую, loss-data, частоту каждой гармоники и параметры сходимости. Адаптер отклоняет отсутствующие витки/B-H-данные, небезопасные key-value строки и несовпадение физических размеров/гармонических phasors между запросом и результатом; unit tests расширены на provenance и negative paths. Требуется локальная Windows Debug/Release проверка.

В репозитории уже существуют application dependency graph/execution API, domain physical geometry, OCCT three-limb core/assembly builders и Qt CAD viewport. Desktop UI пока не вызывает полный application execution pipeline и не формирует согласованный магнитный/тепловой result snapshot. При этом предварительный CAD-путь уже подключён от Construction inputs через строгий physical-geometry adapter к OCCT presentation, semantic component tree и STEP B-Rep export. Следующая работа — закрыть недостающие физические входы/модели, подключить реальное выполнение стадий расчёта и проверить весь путь сборкой и тестами.

### 3D presentation — component palette and surface shading

- [x] Added a presentation-only RGB palette to each flattened CAD display item.
- [x] Assigned distinct semantic preview colors to core segments (steel blue-grey), bobbins (warm insulating beige), and winding envelopes (copper).
- [x] Applied per-component colors in the OCCT AIS viewport and explicitly selected shaded display mode; OCCT viewer lighting provides orientation-dependent face shading.
- [x] Propagate pinned material revision IDs into physical geometry and CAD display items; retain revision provenance on each component.
- [x] Resolve a presentation-only material finish by pinned revision/semantic role: satin electrical steel, polished copper, matte insulating former, neutral matte fallback. This is a visual classification only and does not alter engineering material data.
- [x] Apply per-component OCCT `Graphic3d_MaterialAspect` ambient, diffuse, specular and shininess parameters in shaded mode; steel uses restrained highlights, copper stronger warm highlights, and insulation a low-gloss response.
- [ ] Add E2E assertions for material-finish assignment and a visual/manual viewport check on Windows. Current workflow E2E confirms geometry/export but does not inspect rendered pixels or GPU material response.
- [x] Add per-face tonal shading in the OCCT viewport using each B-Rep face normal and a restrained directional ramp; use AIS_ColoredShape custom face colors while retaining the component-level material finish. This is a face-level presentation gradient, not a continuous texture/shader gradient and does not modify the B-Rep or STEP geometry.
- [ ] Add E2E assertions for face/material presentation metadata and perform a manual Windows viewport review; the current UI E2E does not inspect GPU-rendered pixels. The user confirmed Windows/MSVC Debug E2E after per-face shading: CTest 1/1 PASS (37.51 s); this validates build/workflow regression only, not visual appearance.
- [ ] Consider continuous UV texture/shader gradients only if the manual viewport review shows a concrete need; avoid adding a shader pipeline solely for decoration.

- [x] Propagated explicitly selected core, conductor and insulation material revision IDs from Construction inputs into the physical geometry model; bobbin material identifier remains attached to each bobbin.
- [x] CAD display items now carry material-revision provenance. Known seeded revisions resolve to their corresponding steel, copper and Nomex preview palette; unrecognized or absent revisions retain the semantic fallback palette and are not guessed.
- [ ] Insulation currently has no independent physical solid in the simplified winding-envelope CAD builder, so its revision is preserved as provenance but cannot yet be rendered as a separate visible material layer.
