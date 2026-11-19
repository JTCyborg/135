#pragma once

#include "p135/infrastructure/sqlite/sqlite_material_revision_store.hpp"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QTableWidget;

class MaterialDatabaseWidget final : public QWidget
{
    Q_OBJECT
public:
    explicit MaterialDatabaseWidget(
        p135::infrastructure::sqlite::SqliteMaterialRevisionStore& store,
        QWidget* parent = nullptr);

private:
    void reload();
    void createOrEditMaterial();
    void compareSelectedRevisions();
    void restoreSelectedRevision();
    void toggleArchiveSelected();
    QString selectedRevisionId() const;

    p135::infrastructure::sqlite::SqliteMaterialRevisionStore& store_;
    QLineEdit* search_{};
    QComboBox* recordType_{};
    QCheckBox* includeArchived_{};
    QTableWidget* table_{};
    QLabel* status_{};
};
