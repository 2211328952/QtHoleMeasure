#include "src/LPVDisplayWidget.h"

#include <QApplication>
#include <QTimer>

#include <cstdio>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    LPVDisplayWidget display;
    if (!display.createDisplayControl()) {
        std::fprintf(stderr, "createDisplayControl failed: %s\n", display.lastError().toLocal8Bit().constData());
        return 2;
    }
    if (!display.displayControl()) {
        std::fprintf(stderr, "query ILDisplay failed\n");
        return 3;
    }

    QTimer::singleShot(0, &app, SLOT(quit()));
    return app.exec();
}
