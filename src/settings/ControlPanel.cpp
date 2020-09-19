#include "ControlPanel.hpp"

#include <numeric>

ControlPanel::ControlPanel(QWidget* parent) : QWidget(parent) {
    layout = new QGridLayout(this);
    layout->addWidget(&frame_time_label, 0, 0);
    layout->addWidget(&average_frame_time_label, 1, 0);
}

void ControlPanel::update_frame_time(int ms) {
    frame_time_label.setText("Frame Time: " + QString::number(ms));

    frame_times[current_frame_time_index] = ms;
    int frame_time_array_size = sizeof(frame_times)/sizeof(frame_times[0]);
    current_frame_time_index = (current_frame_time_index+1) % (frame_time_array_size);
    float average_frame_time = std::accumulate(frame_times, frame_times+frame_time_array_size, 0) / float(frame_time_array_size);

    average_frame_time_label.setText("Average Frame Time: " + QString::number(average_frame_time));
}