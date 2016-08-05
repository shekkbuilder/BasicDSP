#include <QCoreApplication>
#include <QDebug>
#include <sstream>
#include "reader.h"
#include "tokenizer.h"
#include "parser.h"
#include "asttovm.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "pa_ringbuffer.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    /** set source editor font */
    const QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    ui->sourceEditor->setFont(fixedFont);

    /** add four sliders programatically */
    m_slider1 = new NamedSlider("slider 1", this);
    ui->mainLayout->addWidget(m_slider1);
    m_slider2 = new NamedSlider("slider 2", this);
    ui->mainLayout->addWidget(m_slider2);
    m_slider3 = new NamedSlider("slider 3", this);
    ui->mainLayout->addWidget(m_slider3);
    m_slider4 = new NamedSlider("slider 4", this);
    ui->mainLayout->addWidget(m_slider4);

    connect(m_slider1, SIGNAL(valueChanged(float)), this, SLOT(on_Slider1Changed(float)));
    connect(m_slider2, SIGNAL(valueChanged(float)), this, SLOT(on_Slider2Changed(float)));
    connect(m_slider3, SIGNAL(valueChanged(float)), this, SLOT(on_Slider3Changed(float)));
    connect(m_slider4, SIGNAL(valueChanged(float)), this, SLOT(on_Slider4Changed(float)));

    /** add VU meters */
    m_leftVUMeter = new VUMeter("Left", this);
    ui->mainLayout->addWidget(m_leftVUMeter);
    m_rightVUMeter = new VUMeter("Right", this);
    ui->mainLayout->addWidget(m_rightVUMeter);

    /** connect the source selection buttons to their handler */
    connect(ui->inputAudioFile, SIGNAL(clicked(bool)), this, SLOT(on_SourceChanged()));
    connect(ui->inputQuadSine, SIGNAL(clicked(bool)), this, SLOT(on_SourceChanged()));
    connect(ui->inputImpulse, SIGNAL(clicked(bool)), this, SLOT(on_SourceChanged()));
    connect(ui->inputSineWave, SIGNAL(clicked(bool)), this, SLOT(on_SourceChanged()));
    connect(ui->inputWhiteNoise, SIGNAL(clicked(bool)), this, SLOT(on_SourceChanged()));
    connect(ui->inputSoundcard, SIGNAL(clicked(bool)), this, SLOT(on_SourceChanged()));

    /** create the virtual machine */
    m_machine = new VirtualMachine(this);

    /** create a GUI timer to update VU etc */
    m_guiTimer = new QTimer(this);
    connect(m_guiTimer, SIGNAL(timeout()), this, SLOT(on_GUITimer()));
    m_guiTimer->start(100);

    /** create a spectrum window */
    m_spectrum = new SpectrumWindow(this);
    m_spectrum->show();

    /** create a scope window */
    m_scope = new ScopeWindow(this);
    m_scope->show();
}

MainWindow::~MainWindow()
{
    if (m_machine != 0)
        m_machine->stop();

    delete ui;
    delete m_spectrum;
}

void MainWindow::on_GUITimer()
{
    float L,R;
    m_machine->getVU(L,R);
    m_leftVUMeter->setLevel(L);
    m_rightVUMeter->setLevel(R);
    m_leftVUMeter->update();
    m_rightVUMeter->update();

    PaUtilRingBuffer* rbPtr = m_machine->getRingBufferPtr(0);

    ring_buffer_size_t items = PaUtil_GetRingBufferReadAvailable(rbPtr);
    while (items >= 256)
    {
        float data[256];
        PaUtil_ReadRingBuffer(rbPtr, data, 256);
        m_scope->submit256Samples(data);
        items = PaUtil_GetRingBufferReadAvailable(rbPtr);
    }
    m_scope->update();
}

void MainWindow::on_actionExit_triggered()
{
    //TODO: check for unsaved things
    //      quit any running threads.
    QCoreApplication::exit(0);
}

void MainWindow::on_runButton_clicked()
{
    Parser    parser;
    Tokenizer tokenizer;

    Reader *reader = Reader::create(ui->sourceEditor->toPlainText());
    if (reader == 0)
    {
        // error, probably due to an empty source code editor
        qDebug() << "Error: no source code";
        return;
    }

    std::vector<token_t> tokens;
    bool ok = tokenizer.process(reader, tokens);
    if (!ok)
    {
        // tokenize error
        qDebug() << "Tokenizer error: " << tokenizer.getErrorString().c_str();
    }
    else
    {
        qDebug() << "Tokenizer produced " << tokens.size() << " tokens";
    }

    qDebug() << "-- TOKENS -- ";
    for(uint32_t i=0; i<tokens.size(); i++)
    {
        qDebug() << tokens[i].tokID;
    }

    statements_t statements;
    bool parseOK = parser.process(tokens, statements);


    // write AST to the debug console
    qDebug() << "-- PARSE TREE -- ";
    std::stringstream ss;
    for(uint32_t i=0; i<statements.size(); i++)
    {
        statements[i]->dump(ss,0);
    }
    qDebug() << ss.str().c_str();


    if (!parseOK)
    {
        Reader::position_info pos = parser.getLastErrorPos();
        QString txt = QString("Program error on line %1: ").arg(pos.line+1);
        txt.append(parser.getLastError().c_str());
        ui->statusBar->showMessage(txt);
    }
    else
    {
        ui->statusBar->showMessage("Program accepted!");
    }


    VM::program_t program;
    VM::variables_t vars;
    if (!ASTToVM::process(statements, program, vars))
    {
        qDebug() << "AST conversion failed! :(";
    }
    else
    {
        ss.clear();
        if (m_machine != 0)
        {
            // dump the program for debugging and run!
            std::stringstream ss;
            m_machine->stop();
            m_machine->loadProgram(program, vars);
            m_machine->dump(ss);
            m_machine->setSlider(0, m_slider1->getValue());
            m_machine->setSlider(1, m_slider2->getValue());
            m_machine->setSlider(2, m_slider3->getValue());
            m_machine->setSlider(3, m_slider4->getValue());
            m_machine->start();
            qDebug() << ss.str().c_str();
            qDebug() << " - Variables -";
            for(size_t i=0; i<vars.size(); i++)
            {
                qDebug() << vars[i].name.c_str();
            }
        }
    }
}

void MainWindow::on_Slider1Changed(float value)
{
    if (m_machine != 0)
    {
        m_machine->setSlider(0,value);
    }
}

void MainWindow::on_Slider2Changed(float value)
{
    if (m_machine != 0)
    {
        m_machine->setSlider(1,value);
    }
}

void MainWindow::on_Slider3Changed(float value)
{
    if (m_machine != 0)
    {
        m_machine->setSlider(2,value);
    }
}

void MainWindow::on_Slider4Changed(float value)
{
    if (m_machine != 0)
    {
        m_machine->setSlider(3,value);
    }
}

void MainWindow::on_SourceChanged()
{
    if (m_machine == 0)
        return;

    if (ui->inputAudioFile->isChecked())
        m_machine->setSource(VirtualMachine::SRC_WAV);
    if (ui->inputSineWave->isChecked())
        m_machine->setSource(VirtualMachine::SRC_SINE);
    if (ui->inputQuadSine->isChecked())
        m_machine->setSource(VirtualMachine::SRC_QUADSINE);
    if (ui->inputWhiteNoise->isChecked())
        m_machine->setSource(VirtualMachine::SRC_NOISE);
    if (ui->inputImpulse->isChecked())
        m_machine->setSource(VirtualMachine::SRC_IMPULSE);
    if (ui->inputSoundcard->isChecked())
        m_machine->setSource(VirtualMachine::SRC_SOUNDCARD);
}

void MainWindow::on_stopButton_clicked()
{
    if (m_machine!=0)
    {
        m_machine->stop();
    }
}