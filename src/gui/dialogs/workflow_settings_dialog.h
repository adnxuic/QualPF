#pragma once
#include "qualpf/application/workflow_settings.h"
#include <QDialog>
#include <QStringList>
#include <functional>
#include <array>
#include <vector>
namespace qualpf::application { class Workspace; }
class QListWidget;
class QStackedWidget;
class QTabWidget;
class QFormLayout;
class QLabel;
class QCheckBox;
class QSpinBox;
class QComboBox;
class QLineEdit;
namespace qualpf::gui {
class WorkflowSettingsDialog final : public QDialog {
    Q_OBJECT
  public:
    WorkflowSettingsDialog(application::Workspace &, QWidget *parent=nullptr);
    bool load(application::WorkflowId, const QString &module={});
    application::WorkflowId selectedWorkflow() const;
    QString selectedModule() const;
  private:
    struct Field {
        application::WorkflowId flow;
        int page;
        QWidget *control;
        QLabel *error;
        std::function<void()> read;
        std::function<void()> write;
        std::function<QString()> check;
    };
    struct Page {QString name; QWidget *widget; QFormLayout *form; QLabel *error;};
    using Profile=application::WorkflowProfile;
    void ensureFlow(application::WorkflowId);
    void buildFlow(application::WorkflowId);
    QCheckBox *boolean(application::WorkflowId,int,const QString &,const QString &,std::function<bool(const Profile &)>,std::function<void(Profile &,bool)>);
    QSpinBox *integer(application::WorkflowId,int,const QString &,const QString &,int,int,std::function<int(const Profile &)>,std::function<void(Profile &,int)>);
    QLineEdit *number(application::WorkflowId,int,const QString &,const QString &,double,double,std::function<double(const Profile &)>,std::function<void(Profile &,double)>);
    QComboBox *choice(application::WorkflowId,int,const QString &,const QString &,QStringList,std::function<int(const Profile &)>,std::function<void(Profile &,int)>);
    Field &addField(application::WorkflowId,int,const QString &,const QString &,QWidget *);
    void note(application::WorkflowId,int,const QString &);
    void refresh();
    void refresh(application::WorkflowId);
    bool collect();
    void restorePage();
    void save();
    void copyPage(Profile &,const Profile &,const QString &) const;
    application::Workspace &workspace_;
    application::ProjectSettings draft_;
    QListWidget *flows_;
    QStackedWidget *stack_;
    QLabel *status_;
    std::array<QTabWidget *,4> tabs_{};
    std::array<std::vector<Page>,4> pages_;
    std::vector<Field> fields_;
};
}
