#ifndef CONTROL_PANEL_HPP
#define CONTROL_PANEL_HPP

#include <QWidget>
#include <QLabel>
#include <QGridLayout>

class ControlPanel : public QWidget {
    Q_OBJECT;

public:
    ControlPanel(QWidget* parent=nullptr);

    void update_frame_time(int ms);

private:
    QGridLayout* layout;

    QLabel frame_time_label;
    QLabel average_frame_time_label;
    size_t current_frame_time_index = 0;
    int frame_times[50] = {};
};

#endif