/*

  Spectrum display widget

  Copyright 2006-2016
  Niels A. Moseley
  Pieter-Tjerk de Boer

  License: GPLv2

*/

#ifndef spectrumwidget_h
#define spectrumwidget_h

#include <stdint.h>
#include <vector>
#include <QWidget>
#include <QImage>
#include "virtualmachine.h"

class SpectrumWidget : public QWidget
{
    Q_OBJECT
public:
    SpectrumWidget(QWidget *parent);

    /** submit 256 FFT'd samples */
    void submit256Samples(const VirtualMachine::ring_buffer_data_t *samples);

protected:
    void paintEvent(QPaintEvent *event);

    int32_t db2pix(float db);
    int32_t x2pix(float xvalue);

    std::vector<VirtualMachine::ring_buffer_data_t>  m_signal;

    float m_dbmin,m_dbmax;
    float m_fmin,m_fmax;

    QImage *m_bkbuffer;
};


#endif
