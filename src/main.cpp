#include "QtHoleMeasureWidget.h"

#include <QtWidgets/QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QtHoleMeasureWidget widget;
    widget.show();
    return app.exec();
}
