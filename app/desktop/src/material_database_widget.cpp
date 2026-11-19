#include "material_database_widget.hpp"

#include "p135/application/material_revision_use_cases.hpp"
#include "p135/domain/component_catalog.hpp"
#include "p135/infrastructure/sqlite/canonical_json.hpp"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QJsonArray>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <set>

using namespace p135;
using namespace p135::application;
using namespace p135::infrastructure::sqlite;

namespace {
std::string utcNow()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();
}

std::string newRevisionId()
{
    return "REV-" + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
}


QString errorText(const Error& error)
{
    return QString::fromStdString(error.message);
}

QDoubleSpinBox* addNumber(QFormLayout* form, const QString& label,
                          double value, double minimum, double maximum,
                          int decimals, const QString& suffix = {})
{
    auto* editor = new QDoubleSpinBox(form->parentWidget());
    editor->setRange(minimum, maximum);
    editor->setDecimals(decimals);
    editor->setValue(value);
    editor->setSuffix(suffix);
    editor->setMaximumWidth(220);
    form->addRow(label, editor);
    return editor;
}

QString typedComponentSnapshot(const QString& recordType, QWidget* parent,
                              const QString& defaultId)
{
    using namespace p135::domain;
    QDialog dialog(parent);
    dialog.setWindowTitle(recordType == "capacitor" ?
        QObject::tr("Typed capacitor editor") : QObject::tr("Typed resistor editor"));
    dialog.resize(620, 720);
    auto* form = new QFormLayout(&dialog);
    auto* id = new QLineEdit(defaultId, &dialog);
    if (!defaultId.isEmpty()) id->setReadOnly(true);
    auto* manufacturer = new QLineEdit(&dialog);
    auto* series = new QLineEdit(&dialog);
    auto* partNumber = new QLineEdit(&dialog);
    form->addRow(QObject::tr("Database ID"), id);
    form->addRow(QObject::tr("Manufacturer"), manufacturer);
    form->addRow(QObject::tr("Series"), series);
    form->addRow(QObject::tr("Part number"), partNumber);

    QJsonObject component;
    const bool isCapacitor = recordType == "capacitor";
    QDoubleSpinBox *value1{}, *value2{}, *value3{}, *value4{}, *minimum{}, *maximum{};
    if (isCapacitor) {
        value1 = addNumber(form, QObject::tr("Capacitance"), 10.0, 0.000001, 1.0e9, 6, " µF");
        value2 = addNumber(form, QObject::tr("Tolerance"), 5.0, 0.0, 99.0, 3, " %");
        value3 = addNumber(form, QObject::tr("Rated DC voltage"), 1000.0, 0.001, 1.0e9, 3, " V");
        value4 = addNumber(form, QObject::tr("Scalar ESR (optional)"), 0.0, 0.0, 1.0e9, 6, " mΩ");
        auto* constantEsr = new QCheckBox(QObject::tr("Datasheet confirms ESR is frequency-independent"), &dialog);
        form->addRow(QObject::tr("Scalar ESR model"), constantEsr);
        auto* esrCurve = new QPlainTextEdit(&dialog);
        esrCurve->setPlaceholderText(QObject::tr("One point per line: frequency_Hz, ESR_mOhm\nExample: 50, 12.5\nNo extrapolation is performed outside the entered range."));
        esrCurve->setMaximumHeight(120);
        form->addRow(QObject::tr("Frequency-dependent ESR curve"), esrCurve);
        auto* esl = addNumber(form, QObject::tr("ESL (optional)"), 0.0, 0.0, 1.0e9, 6, " nH");
        auto* ripple = addNumber(form, QObject::tr("Ripple current (optional)"), 0.0, 0.0, 1.0e9, 3, " A");
        minimum = addNumber(form, QObject::tr("Minimum temperature"), -40.0, -273.14, 1000.0, 2, " °C");
        maximum = addNumber(form, QObject::tr("Maximum temperature"), 105.0, -272.14, 2000.0, 2, " °C");
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
        form->addRow(buttons);
        QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted) return {};
        CapacitorComponent model;
        model.id = id->text().trimmed().toStdString();
        model.manufacturer = manufacturer->text().trimmed().toStdString();
        model.series = series->text().trimmed().toStdString();
        model.partNumber = partNumber->text().trimmed().toStdString();
        model.dataSource = MaterialDataSource::UserDefined;
        model.sourceReference = "user-entered-unverified";
        model.datasetRevision = "user-entered";
        model.dielectric = CapacitorDielectric::PolypropyleneFilm;
        model.technology = CapacitorTechnology::MetallizedFilm;
        model.capacitance = units::Capacitance{value1->value() * 1.0e-6};
        model.toleranceFraction = value2->value() / 100.0;
        model.ratedVoltage = units::Voltage{value3->value()};
        model.esr = value4->value() > 0.0 ?
            std::optional<units::Resistance>{units::Resistance{value4->value() * 1.0e-3}} : std::nullopt;
        model.esrConfirmedFrequencyIndependent = constantEsr->isChecked();
        const auto curveLines = esrCurve->toPlainText().split('\n');
        for (const auto& rawLine : curveLines) {
            const QString line = rawLine.trimmed();
            if (line.isEmpty() || line.startsWith('#')) continue;
            const auto columns = line.split(',', Qt::KeepEmptyParts);
            if (columns.size() != 2) {
                QMessageBox::warning(parent, QObject::tr("Invalid ESR curve"),
                    QObject::tr("Each ESR curve line must contain frequency_Hz,ESR_mOhm."));
                return {};
            }
            bool frequencyOk = false;
            bool esrOk = false;
            const double frequencyHz = columns[0].trimmed().toDouble(&frequencyOk);
            const double esrMilliOhm = columns[1].trimmed().toDouble(&esrOk);
            if (!frequencyOk || !esrOk || !std::isfinite(frequencyHz) ||
                !std::isfinite(esrMilliOhm) || frequencyHz <= 0.0 || esrMilliOhm <= 0.0) {
                QMessageBox::warning(parent, QObject::tr("Invalid ESR curve"),
                    QObject::tr("ESR curve frequency and resistance must be finite and positive."));
                return {};
            }
            model.esrFrequencyCurve.push_back({
                units::Frequency{frequencyHz},
                units::Resistance{esrMilliOhm * 1.0e-3}
            });
        }
        model.esl = esl->value() > 0.0 ?
            std::optional<units::Inductance>{units::Inductance{esl->value() * 1.0e-9}} : std::nullopt;
        model.rippleCurrent = ripple->value() > 0.0 ?
            std::optional<units::Current>{units::Current{ripple->value()}} : std::nullopt;
        model.minimumTemperature = units::TemperatureKelvin{minimum->value() + 273.15};
        model.maximumTemperature = units::TemperatureKelvin{maximum->value() + 273.15};
        const auto checked = validate(model);
        if (!checked) {
            QMessageBox::warning(parent, QObject::tr("Invalid capacitor"), errorText(checked.error()));
            return {};
        }
        component = QJsonObject{
            {QStringLiteral("id"), QString::fromStdString(model.id)}, {QStringLiteral("manufacturer"), QString::fromStdString(model.manufacturer)},
            {QStringLiteral("series"), QString::fromStdString(model.series)}, {QStringLiteral("partNumber"), QString::fromStdString(model.partNumber)},
            {QStringLiteral("technology"), QStringLiteral("metallized-polypropylene-film")},
            {QStringLiteral("capacitance_F"), model.capacitance.si()},
            {QStringLiteral("tolerance_fraction"), model.toleranceFraction},
            {QStringLiteral("ratedVoltageDC_V"), model.ratedVoltage.si()},
            {QStringLiteral("minimumTemperature_K"), model.minimumTemperature.si()},
            {QStringLiteral("maximumTemperature_K"), model.maximumTemperature.si()},
        };
        if (model.esr) component.insert(QStringLiteral("esr_ohm"), model.esr->si());
        component.insert(QStringLiteral("esr_frequency_independent_confirmed"),
                         model.esrConfirmedFrequencyIndependent);
        if (!model.esrFrequencyCurve.empty()) {
            QJsonArray curve;
            for (const auto& point : model.esrFrequencyCurve) {
                curve.append(QJsonObject{
                    {QStringLiteral("frequency_Hz"), point.frequency.si()},
                    {QStringLiteral("esr_ohm"), point.esr.si()}
                });
            }
            component.insert(QStringLiteral("esr_frequency_curve"), curve);
        }
        if (model.esl) component.insert(QStringLiteral("esl_H"), model.esl->si());
        if (model.rippleCurrent) component.insert(QStringLiteral("rippleCurrent_A"), model.rippleCurrent->si());
    } else {
        value1 = addNumber(form, QObject::tr("Resistance"), 10.0, 0.000001, 1.0e12, 6, " Ω");
        value2 = addNumber(form, QObject::tr("Tolerance"), 1.0, 0.0, 99.0, 3, " %");
        value3 = addNumber(form, QObject::tr("Rated power"), 50.0, 0.001, 1.0e9, 3, " W");
        value4 = addNumber(form, QObject::tr("Maximum working voltage"), 1000.0, 0.001, 1.0e9, 3, " V");
        auto* tcr = addNumber(form, QObject::tr("TCR (optional)"), 0.0, -1.0e9, 1.0e9, 3, " ppm/K");
        minimum = addNumber(form, QObject::tr("Minimum temperature"), -55.0, -273.14, 1000.0, 2, " °C");
        maximum = addNumber(form, QObject::tr("Maximum temperature"), 200.0, -272.14, 2000.0, 2, " °C");
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
        form->addRow(buttons);
        QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted) return {};
        ResistorComponent model;
        model.id = id->text().trimmed().toStdString();
        model.manufacturer = manufacturer->text().trimmed().toStdString();
        model.series = series->text().trimmed().toStdString();
        model.partNumber = partNumber->text().trimmed().toStdString();
        model.dataSource = MaterialDataSource::UserDefined;
        model.sourceReference = "user-entered-unverified";
        model.datasetRevision = "user-entered";
        model.technology = ResistorTechnology::Wirewound;
        model.resistance = units::Resistance{value1->value()};
        model.toleranceFraction = value2->value() / 100.0;
        model.ratedPower = units::Power{value3->value()};
        model.maximumWorkingVoltage = units::Voltage{value4->value()};
        if (tcr->value() != 0.0)
            model.temperatureCoefficient = units::TemperatureCoefficient{tcr->value() * 1.0e-6};
        model.minimumTemperature = units::TemperatureKelvin{minimum->value() + 273.15};
        model.maximumTemperature = units::TemperatureKelvin{maximum->value() + 273.15};
        const auto checked = validate(model);
        if (!checked) {
            QMessageBox::warning(parent, QObject::tr("Invalid resistor"), errorText(checked.error()));
            return {};
        }
        component = QJsonObject{
            {QStringLiteral("id"), QString::fromStdString(model.id)},
            {QStringLiteral("manufacturer"), QString::fromStdString(model.manufacturer)},
            {QStringLiteral("series"), QString::fromStdString(model.series)},
            {QStringLiteral("partNumber"), QString::fromStdString(model.partNumber)},
            {QStringLiteral("technology"), QStringLiteral("wirewound")},
            {QStringLiteral("resistance_ohm"), model.resistance.si()},
            {QStringLiteral("tolerance_fraction"), model.toleranceFraction},
            {QStringLiteral("ratedPower_W"), model.ratedPower.si()},
            {QStringLiteral("maximumWorkingVoltage_V"), model.maximumWorkingVoltage.si()},
            {QStringLiteral("minimumTemperature_K"), model.minimumTemperature.si()},
            {QStringLiteral("maximumTemperature_K"), model.maximumTemperature.si()},
        };
        if (model.temperatureCoefficient)
            component.insert(QStringLiteral("temperatureCoefficient_per_K"),
                             model.temperatureCoefficient->si());
    }
    const QJsonObject snapshot{
        {QStringLiteral("schemaVersion"), 1}, {QStringLiteral("component"), component},
        {QStringLiteral("dataQuality"), QJsonObject{
            {QStringLiteral("sourceStatus"), QStringLiteral("USER_ENTERED_UNVERIFIED")},
            {QStringLiteral("designReleaseStatus"), QStringLiteral("NOT_VERIFIED")},
            {QStringLiteral("notes"), QStringLiteral("Typed structural validation passed. Verify the entered values against the cited current manufacturer datasheet before design release.")}
        }}
    };
    return QString::fromUtf8(QJsonDocument(snapshot).toJson(QJsonDocument::Indented));
}
}

MaterialDatabaseWidget::MaterialDatabaseWidget(SqliteMaterialRevisionStore& store, QWidget* parent)
    : QWidget(parent), store_(store)
{
    auto* layout = new QVBoxLayout(this);
    auto* heading = new QLabel(tr("Material database"));
    heading->setStyleSheet("font-size: 20px; font-weight: 600;");
    layout->addWidget(heading);

    auto* filters = new QHBoxLayout;
    search_ = new QLineEdit;
    search_->setPlaceholderText(tr("Search by material ID or snapshot content"));
    recordType_ = new QComboBox;
    recordType_->addItem(tr("All record types"), QString{});
    recordType_->addItem(tr("Core material"), "core-material");
    recordType_->addItem(tr("Conductor"), "conductor-material");
    recordType_->addItem(tr("Insulation"), "insulation-material");
    recordType_->addItem(tr("Capacitor"), "capacitor");
    recordType_->addItem(tr("Resistor"), "resistor");
    includeArchived_ = new QCheckBox(tr("Include archived"));
    filters->addWidget(search_, 1);
    filters->addWidget(recordType_);
    filters->addWidget(includeArchived_);
    layout->addLayout(filters);

    table_ = new QTableWidget(0, 6);
    table_->setHorizontalHeaderLabels({
        tr("Material ID"), tr("Type"), tr("Revision"), tr("State"),
        tr("SHA-256"), tr("Updated (UTC)")
    });
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(table_, 1);

    auto* actions = new QHBoxLayout;
    auto* create = new QPushButton(tr("New material"));
    auto* edit = new QPushButton(tr("New revision / edit"));
    auto* history = new QPushButton(tr("Revision history"));
    auto* compare = new QPushButton(tr("Compare selected (2)"));
    auto* restore = new QPushButton(tr("Restore selected revision"));
    auto* archive = new QPushButton(tr("Archive / unarchive"));
    auto* refresh = new QPushButton(tr("Refresh"));
    actions->addWidget(create);
    actions->addWidget(edit);
    actions->addWidget(history);
    actions->addWidget(compare);
    actions->addWidget(restore);
    actions->addWidget(archive);
    actions->addStretch();
    actions->addWidget(refresh);
    layout->addLayout(actions);

    status_ = new QLabel(tr("Material catalog ready."));
    status_->setWordWrap(true);
    layout->addWidget(status_);

    connect(search_, &QLineEdit::textChanged, this, [this] { reload(); });
    connect(recordType_, &QComboBox::currentIndexChanged, this, [this] { reload(); });
    connect(includeArchived_, &QCheckBox::toggled, this, [this] { reload(); });
    connect(create, &QPushButton::clicked, this, [this] {
        table_->clearSelection();
        createOrEditMaterial();
    });
    connect(edit, &QPushButton::clicked, this, [this] { createOrEditMaterial(); });
    connect(table_, &QTableWidget::cellDoubleClicked, this,
            [this](int, int) { createOrEditMaterial(); });
    connect(history, &QPushButton::clicked, this, [this] { showRevisionHistory(); });
    connect(compare, &QPushButton::clicked, this, [this] { compareSelectedRevisions(); });
    connect(restore, &QPushButton::clicked, this, [this] { restoreSelectedRevision(); });
    connect(archive, &QPushButton::clicked, this, [this] { toggleArchiveSelected(); });
    connect(refresh, &QPushButton::clicked, this, [this] { reload(); });

    reload();
}

void MaterialDatabaseWidget::reload()
{
    MaterialCatalogFilter filter;
    filter.recordType = recordType_->currentData().toString().toStdString();
    filter.searchText = search_->text().toStdString();
    filter.includeArchived = includeArchived_->isChecked();
    filter.pageSize = 500;
    const auto result = store_.searchLatestRevisions(filter);
    if (!result) {
        status_->setText(tr("Catalog query failed: %1").arg(errorText(result.error())));
        return;
    }

    table_->setRowCount(0);
    for (const auto& revision : result.value()) {
        const auto archived = store_.isArchived(revision.materialId);
        if (!archived) {
            status_->setText(tr("Cannot read archive state: %1").arg(errorText(archived.error())));
            return;
        }
        const int row = table_->rowCount();
        table_->insertRow(row);
        auto* id = new QTableWidgetItem(QString::fromStdString(revision.materialId));
        id->setData(Qt::UserRole, QString::fromStdString(revision.revisionId));
        id->setData(Qt::UserRole + 1, QString::fromStdString(revision.materialId));
        table_->setItem(row, 0, id);
        table_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(revision.recordType)));
        table_->setItem(row, 2, new QTableWidgetItem(QString::number(revision.revisionNumber)));
        table_->setItem(row, 3, new QTableWidgetItem(archived.value() ? tr("Archived") : tr("Active")));
        table_->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(revision.contentHash)));
        table_->setItem(row, 5, new QTableWidgetItem(QString::fromStdString(revision.createdAtUtc)));
    }
    status_->setText(tr("%1 material record(s). Showing latest revision per material.")
        .arg(result.value().size()));
}

QString MaterialDatabaseWidget::selectedRevisionId() const
{
    const auto rows = table_->selectionModel()->selectedRows();
    if (rows.size() != 1) return {};
    const auto* item = table_->item(rows.front().row(), 0);
    return item ? item->data(Qt::UserRole).toString() : QString{};
}

void MaterialDatabaseWidget::createOrEditMaterial()
{
    const QString selectedId = selectedRevisionId();
    const bool editing = !selectedId.isEmpty();
    std::optional<MaterialRevision> base;
    if (editing) {
        const auto current = store_.latestRevision(
            table_->item(table_->selectionModel()->selectedRows().front().row(), 0)
                ->data(Qt::UserRole + 1).toString().toStdString());
        if (!current || !current.value()) {
            QMessageBox::warning(this, tr("Material database"),
                current ? tr("Material has no revision.") : errorText(current.error()));
            return;
        }
        base = *current.value();
    }

    QDialog dialog(this);
    dialog.setWindowTitle(editing ? tr("Create material revision") : tr("New material"));
    dialog.resize(720, 560);
    auto* form = new QFormLayout(&dialog);
    auto* id = new QLineEdit(editing ? QString::fromStdString(base->materialId) : QString{});
    id->setPlaceholderText(tr("e.g. MAT-CU-001"));
    if (editing) id->setReadOnly(true);
    auto* type = new QComboBox;
    type->addItem(tr("Core material"), "core-material");
    type->addItem(tr("Conductor"), "conductor-material");
    type->addItem(tr("Insulation"), "insulation-material");
    type->addItem(tr("Capacitor"), "capacitor");
    type->addItem(tr("Resistor"), "resistor");
    if (editing) {
        const int index = type->findData(QString::fromStdString(base->recordType));
        if (index >= 0) type->setCurrentIndex(index);
        type->setEnabled(false);
    }
    auto* source = new QLineEdit(editing ? QString::fromStdString(base->sourceReference) : "user-defined");
    auto* dataset = new QLineEdit(editing ? QString::fromStdString(base->datasetRevision) : "manual-1");
    auto* author = new QLineEdit("local-user");
    auto* payload = new QPlainTextEdit;
    payload->setTabChangesFocus(false);
    payload->setPlaceholderText(tr("Canonical JSON snapshot; engineering values must use SI units."));
    payload->setPlainText(editing ? QString::fromStdString(base->payload) : "{\n  \"name\": \"New material\"\n}");
    form->addRow(tr("Material ID"), id);
    form->addRow(tr("Record type"), type);
    form->addRow(tr("Source reference"), source);
    form->addRow(tr("Dataset revision"), dataset);
    form->addRow(tr("Author"), author);
    form->addRow(tr("Snapshot JSON"), payload);
    auto* typedEditor = new QPushButton(tr("Open typed capacitor / resistor editor"), &dialog);
    form->addRow(typedEditor);
    typedEditor->setEnabled(type->currentData().toString() == "capacitor" ||
                            type->currentData().toString() == "resistor");
    connect(type, &QComboBox::currentIndexChanged, &dialog, [type, typedEditor] {
        const auto selectedType = type->currentData().toString();
        typedEditor->setEnabled(selectedType == "capacitor" || selectedType == "resistor");
    });
    connect(typedEditor, &QPushButton::clicked, &dialog, [type, payload, id, &dialog] {
        const auto json = typedComponentSnapshot(
            type->currentData().toString(), &dialog, id->text().trimmed());
        if (json.isEmpty()) return;
        const auto document = QJsonDocument::fromJson(json.toUtf8());
        const auto componentId = document.object().value(QStringLiteral("component"))
            .toObject().value(QStringLiteral("id")).toString();
        if (!componentId.isEmpty()) id->setText(componentId);
        payload->setPlainText(json);
    });
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;

    const auto canonical = canonicalizeJson(payload->toPlainText().toStdString());
    if (!canonical) {
        QMessageBox::warning(this, tr("Invalid JSON"), errorText(canonical.error()));
        return;
    }
    const auto hash = sha256Hex(canonical.value());
    if (!hash) {
        QMessageBox::warning(this, tr("Snapshot error"), errorText(hash.error()));
        return;
    }
    const std::string materialId = id->text().trimmed().toStdString();
    if (materialId.empty() || source->text().trimmed().isEmpty() ||
        dataset->text().trimmed().isEmpty() || author->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Missing data"),
            tr("Material ID, source, dataset revision and author are required."));
        return;
    }

    MaterialRevision revision;
    revision.revisionId = newRevisionId();
    revision.materialId = materialId;
    revision.revisionNumber = editing ? base->revisionNumber + 1 : 1;
    if (editing) revision.parentRevisionId = base->revisionId;
    revision.recordType = type->currentData().toString().toStdString();
    revision.payload = canonical.value();
    revision.contentHash = hash.value();
    revision.sourceReference = source->text().trimmed().toStdString();
    revision.datasetRevision = dataset->text().trimmed().toStdString();
    revision.author = author->text().trimmed().toStdString();
    revision.createdAtUtc = utcNow();

    CreateMaterialRevision create(store_);
    const auto saved = create.execute(revision);
    if (!saved) {
        QMessageBox::warning(this, tr("Cannot save material"), errorText(saved.error()));
        return;
    }
    reload();
    status_->setText(tr("Saved %1 revision %2.")
        .arg(QString::fromStdString(materialId)).arg(revision.revisionNumber));
}

void MaterialDatabaseWidget::compareSelectedRevisions()
{
    const auto rows = table_->selectionModel()->selectedRows();
    if (rows.size() != 2) {
        QMessageBox::information(this, tr("Compare revisions"),
            tr("Select exactly two material rows to compare their latest revisions."));
        return;
    }
    const auto leftId = table_->item(rows[0].row(), 0)->data(Qt::UserRole).toString().toStdString();
    const auto rightId = table_->item(rows[1].row(), 0)->data(Qt::UserRole).toString().toStdString();
    CompareMaterialRevisions compare(store_);
    const auto result = compare.execute(leftId, rightId);
    if (!result) {
        QMessageBox::warning(this, tr("Compare revisions"), errorText(result.error()));
        return;
    }
    const auto& value = result.value();
    const QString summary = tr(
        "Left: %1 (revision %2)\nRight: %3 (revision %4)\n\n"
        "Snapshot payload: %5\nRecord type: %6\nProvenance: %7\n\n"
        "LEFT SNAPSHOT\n%8\n\nRIGHT SNAPSHOT\n%9")
        .arg(QString::fromStdString(value.left.materialId))
        .arg(value.left.revisionNumber)
        .arg(QString::fromStdString(value.right.materialId))
        .arg(value.right.revisionNumber)
        .arg(value.payloadEqual ? tr("identical") : tr("different"))
        .arg(value.recordTypeEqual ? tr("identical") : tr("different"))
        .arg(value.provenanceEqual ? tr("identical") : tr("different"))
        .arg(QString::fromStdString(value.left.payload))
        .arg(QString::fromStdString(value.right.payload));
    QMessageBox box(this);
    box.setWindowTitle(tr("Material revision comparison"));
    box.setText(summary);
    box.setTextInteractionFlags(Qt::TextSelectableByMouse);
    box.setIcon(QMessageBox::Information);
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}

void MaterialDatabaseWidget::showRevisionHistory()
{
    const auto rows = table_->selectionModel()->selectedRows();
    if (rows.size() != 1) {
        QMessageBox::information(this, tr("Revision history"), tr("Select one material."));
        return;
    }
    const std::string materialId = table_->item(rows.front().row(), 0)
        ->data(Qt::UserRole + 1).toString().toStdString();
    const auto history = store_.revisionHistory(materialId);
    if (!history) {
        QMessageBox::warning(this, tr("Revision history"), errorText(history.error()));
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Revision history — %1").arg(QString::fromStdString(materialId)));
    dialog.resize(900, 480);
    auto* layout = new QVBoxLayout(&dialog);
    auto* revisions = new QTableWidget(0, 5);
    revisions->setHorizontalHeaderLabels({tr("Revision"), tr("Revision ID"), tr("Parent"), tr("Author"), tr("Created (UTC)")});
    revisions->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    revisions->setSelectionBehavior(QAbstractItemView::SelectRows);
    revisions->setSelectionMode(QAbstractItemView::ExtendedSelection);
    revisions->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for (const auto& revision : history.value()) {
        const int row = revisions->rowCount();
        revisions->insertRow(row);
        auto* number = new QTableWidgetItem(QString::number(revision.revisionNumber));
        number->setData(Qt::UserRole, QString::fromStdString(revision.revisionId));
        revisions->setItem(row, 0, number);
        revisions->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(revision.revisionId)));
        revisions->setItem(row, 2, new QTableWidgetItem(revision.parentRevisionId
            ? QString::fromStdString(*revision.parentRevisionId) : QStringLiteral("—")));
        revisions->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(revision.author)));
        revisions->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(revision.createdAtUtc)));
    }
    layout->addWidget(revisions);
    auto* actions = new QHBoxLayout;
    auto* compare = new QPushButton(tr("Compare two revisions"));
    auto* restore = new QPushButton(tr("Restore selected revision"));
    auto* close = new QPushButton(tr("Close"));
    actions->addWidget(compare);
    actions->addWidget(restore);
    actions->addStretch();
    actions->addWidget(close);
    layout->addLayout(actions);
    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(compare, &QPushButton::clicked, &dialog, [this, revisions, &dialog] {
        const auto selectedRows = revisions->selectionModel()->selectedRows();
        if (selectedRows.size() != 2) {
            QMessageBox::information(&dialog, tr("Compare revisions"), tr("Select exactly two revisions."));
            return;
        }
        const auto leftId = revisions->item(selectedRows[0].row(), 0)->data(Qt::UserRole).toString().toStdString();
        const auto rightId = revisions->item(selectedRows[1].row(), 0)->data(Qt::UserRole).toString().toStdString();
        CompareMaterialRevisions compareUseCase(store_);
        const auto result = compareUseCase.execute(leftId, rightId);
        if (!result) {
            QMessageBox::warning(&dialog, tr("Compare revisions"), errorText(result.error()));
            return;
        }
        const auto& value = result.value();
        const QString text = tr("Revision %1 vs %2\nPayload: %3\nRecord type: %4\nProvenance: %5\n\nLEFT\n%6\n\nRIGHT\n%7")
            .arg(value.left.revisionNumber).arg(value.right.revisionNumber)
            .arg(value.payloadEqual ? tr("identical") : tr("different"))
            .arg(value.recordTypeEqual ? tr("identical") : tr("different"))
            .arg(value.provenanceEqual ? tr("identical") : tr("different"))
            .arg(QString::fromStdString(value.left.payload))
            .arg(QString::fromStdString(value.right.payload));
        QMessageBox box(&dialog);
        box.setWindowTitle(tr("Revision comparison"));
        box.setText(text);
        box.setTextInteractionFlags(Qt::TextSelectableByMouse);
        box.exec();
    });
    connect(restore, &QPushButton::clicked, &dialog, [this, revisions, &dialog, materialId] {
        const auto selectedRows = revisions->selectionModel()->selectedRows();
        if (selectedRows.size() != 1) {
            QMessageBox::information(&dialog, tr("Restore revision"), tr("Select exactly one revision."));
            return;
        }
        const auto sourceId = revisions->item(selectedRows.front().row(), 0)
            ->data(Qt::UserRole).toString().toStdString();
        bool accepted = false;
        const QString actor = QInputDialog::getText(&dialog, tr("Restore revision"),
            tr("Restored by"), QLineEdit::Normal, "local-user", &accepted);
        if (!accepted || actor.trimmed().isEmpty()) return;
        const auto archived = store_.isArchived(materialId);
        if (!archived) {
            QMessageBox::warning(&dialog, tr("Restore revision"), errorText(archived.error()));
            return;
        }
        const auto timestamp = utcNow();
        if (archived.value()) {
            const auto unarchived = store_.setArchived(materialId, false, actor.trimmed().toStdString(), timestamp);
            if (!unarchived) {
                QMessageBox::warning(&dialog, tr("Restore revision"), errorText(unarchived.error()));
                return;
            }
        }
        RestoreMaterialRevision restoreUseCase(store_);
        const auto restored = restoreUseCase.execute(sourceId, newRevisionId(), actor.trimmed().toStdString(), timestamp);
        if (!restored) {
            QMessageBox::warning(&dialog, tr("Restore revision"), errorText(restored.error()));
            return;
        }
        QMessageBox::information(&dialog, tr("Restore revision"),
            tr("Restored as revision %1.").arg(restored.value().revisionNumber));
        dialog.accept();
        reload();
    });
    dialog.exec();
}

void MaterialDatabaseWidget::restoreSelectedRevision()
{
    const auto revisionId = selectedRevisionId();
    if (revisionId.isEmpty()) {
        QMessageBox::information(this, tr("Restore revision"), tr("Select one material row."));
        return;
    }
    bool accepted = false;
    const QString actor = QInputDialog::getText(this, tr("Restore revision"),
        tr("Restored by"), QLineEdit::Normal, "local-user", &accepted);
    if (!accepted || actor.trimmed().isEmpty()) return;

    const auto selected = store_.findRevision(revisionId.toStdString());
    if (!selected || !selected.value()) {
        QMessageBox::warning(this, tr("Restore revision"),
            selected ? tr("Selected revision no longer exists.") : errorText(selected.error()));
        return;
    }
    const auto archived = store_.isArchived(selected.value()->materialId);
    if (!archived) {
        QMessageBox::warning(this, tr("Restore revision"), errorText(archived.error()));
        return;
    }
    const auto timestamp = utcNow();
    if (archived.value()) {
        const auto unarchived = store_.setArchived(
            selected.value()->materialId, false, actor.trimmed().toStdString(), timestamp);
        if (!unarchived) {
            QMessageBox::warning(this, tr("Restore revision"), errorText(unarchived.error()));
            return;
        }
    }

    RestoreMaterialRevision restore(store_);
    const auto restored = restore.execute(revisionId.toStdString(), newRevisionId(),
        actor.trimmed().toStdString(), timestamp);
    if (!restored) {
        QMessageBox::warning(this, tr("Restore revision"), errorText(restored.error()));
        return;
    }
    reload();
    status_->setText(tr("Restored as new revision %1.")
        .arg(QString::fromStdString(restored.value().revisionId)));
}

void MaterialDatabaseWidget::toggleArchiveSelected()
{
    const auto revisionId = selectedRevisionId();
    if (revisionId.isEmpty()) {
        QMessageBox::information(this, tr("Archive material"), tr("Select one material row."));
        return;
    }
    const auto selected = store_.findRevision(revisionId.toStdString());
    if (!selected || !selected.value()) {
        QMessageBox::warning(this, tr("Archive material"),
            selected ? tr("Selected revision no longer exists.") : errorText(selected.error()));
        return;
    }
    const auto archived = store_.isArchived(selected.value()->materialId);
    if (!archived) {
        QMessageBox::warning(this, tr("Archive material"), errorText(archived.error()));
        return;
    }
    const bool nextState = !archived.value();
    const QString action = nextState ? tr("archive") : tr("unarchive");
    if (QMessageBox::question(this, tr("Confirm"), tr("Are you sure you want to %1 %2?")
        .arg(action, QString::fromStdString(selected.value()->materialId))) != QMessageBox::Yes)
        return;
    bool accepted = false;
    const QString actor = QInputDialog::getText(this, tr("Audit actor"),
        tr("Changed by"), QLineEdit::Normal, "local-user", &accepted);
    if (!accepted || actor.trimmed().isEmpty()) return;
    const auto result = store_.setArchived(selected.value()->materialId, nextState,
        actor.trimmed().toStdString(), utcNow());
    if (!result) {
        QMessageBox::warning(this, tr("Archive material"), errorText(result.error()));
        return;
    }
    reload();
}
