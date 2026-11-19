#include "material_database_widget.hpp"

#include "p135/application/material_revision_use_cases.hpp"
#include "p135/infrastructure/sqlite/canonical_json.hpp"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
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
    auto* compare = new QPushButton(tr("Compare selected (2)"));
    auto* restore = new QPushButton(tr("Restore selected revision"));
    auto* archive = new QPushButton(tr("Archive / unarchive"));
    auto* refresh = new QPushButton(tr("Refresh"));
    actions->addWidget(create);
    actions->addWidget(edit);
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
