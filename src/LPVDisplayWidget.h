#pragma once

#include "LPVDisplayControl.h"

#include <QAxWidget>
#include <QString>

class LPVDisplayWidget : public QAxWidget
{
public:
    explicit LPVDisplayWidget(QWidget* parent = nullptr);

    bool createDisplayControl();
    ILDisplayPtr displayControl() const;
    QString lastError() const;

protected:
    bool initialize(IUnknown** ptr) override;

private:
    bool createFromDll(IUnknown** ptr);

    QString m_lastError;
};
