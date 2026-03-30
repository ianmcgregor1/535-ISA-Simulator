#include "mainwindow.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QSplitter>
#include <QScrollArea>
#include <QFont>
#include <QFontDatabase>
#include <QPalette>
#include <QStyle>
#include <QFrame>
#include <fstream>
#include <sstream>
#include <cstring>
#include <iomanip>

// ─── SimWorker ────────────────────────────────────────────────────────────────

SimWorker::SimWorker(Memory* cache, Memory* dram, RegisterFile* regs,
                     int count, QObject* parent)
    : QThread(parent), cache(cache), dram(dram), regs(regs), count(count) {}

void SimWorker::run() {
    // Stub: simulate 'count' instructions
    // Replace this body with your real pipeline tick loop, e.g.:
    //   for (int i = 0; i < count; ++i) { pipeline->tick(); emit progress(i+1); }
    for (int i = 0; i < count; ++i) {
        // TODO: pipeline->tick();
        if (i % 100 == 0) emit progress(i);
    }
    emit finished();
}

// ─── MainWindow ───────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    // Dark terminal aesthetic — suits a CPU simulator
    setStyleSheet(R"(
        QMainWindow, QWidget {
            background-color: #0d1117;
            color: #c9d1d9;
            font-family: 'JetBrains Mono', 'Fira Code', 'Consolas', monospace;
            font-size: 12px;
        }
        QTabWidget::pane {
            border: 1px solid #30363d;
            background: #0d1117;
        }
        QTabBar::tab {
            background: #161b22;
            color: #8b949e;
            padding: 6px 18px;
            border: 1px solid #30363d;
            border-bottom: none;
            font-size: 11px;
            letter-spacing: 1px;
            text-transform: uppercase;
        }
        QTabBar::tab:selected {
            background: #0d1117;
            color: #58a6ff;
            border-top: 2px solid #58a6ff;
        }
        QTableWidget {
            background-color: #0d1117;
            alternate-background-color: #161b22;
            gridline-color: #21262d;
            color: #c9d1d9;
            border: none;
            selection-background-color: #1f3a5f;
            selection-color: #ffffff;
        }
        QTableWidget::item { padding: 2px 6px; }
        QHeaderView::section {
            background-color: #161b22;
            color: #58a6ff;
            border: none;
            border-right: 1px solid #21262d;
            border-bottom: 1px solid #30363d;
            padding: 4px 6px;
            font-weight: bold;
            letter-spacing: 1px;
            font-size: 11px;
        }
        QPushButton {
            background-color: #21262d;
            color: #c9d1d9;
            border: 1px solid #30363d;
            padding: 6px 14px;
            border-radius: 4px;
            font-size: 11px;
            letter-spacing: 1px;
        }
        QPushButton:hover  { background-color: #30363d; border-color: #58a6ff; color: #58a6ff; }
        QPushButton:pressed{ background-color: #1f3a5f; }
        QPushButton#runBtn {
            background-color: #1a4731;
            border-color: #2ea043;
            color: #3fb950;
            font-weight: bold;
        }
        QPushButton#runBtn:hover { background-color: #2ea043; color: #ffffff; }
        QPushButton#stepBtn {
            background-color: #1f3a5f;
            border-color: #388bfd;
            color: #58a6ff;
            font-weight: bold;
        }
        QPushButton#stepBtn:hover { background-color: #388bfd; color: #ffffff; }
        QPushButton#resetBtn {
            background-color: #3d1f1f;
            border-color: #f85149;
            color: #f85149;
        }
        QPushButton#resetBtn:hover { background-color: #f85149; color: #ffffff; }
        QComboBox {
            background-color: #21262d;
            border: 1px solid #30363d;
            color: #c9d1d9;
            padding: 4px 8px;
            border-radius: 4px;
        }
        QComboBox:hover { border-color: #58a6ff; }
        QComboBox QAbstractItemView {
            background-color: #161b22;
            border: 1px solid #30363d;
            color: #c9d1d9;
            selection-background-color: #1f3a5f;
        }
        QSpinBox {
            background-color: #21262d;
            border: 1px solid #30363d;
            color: #c9d1d9;
            padding: 4px 8px;
            border-radius: 4px;
        }
        QLabel#fileLabel {
            color: #8b949e;
            font-size: 11px;
            padding: 4px 8px;
            background: #161b22;
            border: 1px solid #21262d;
            border-radius: 4px;
        }
        QLabel#cycleLabel {
            color: #3fb950;
            font-size: 11px;
            font-weight: bold;
            padding: 4px 10px;
            background: #161b22;
            border: 1px solid #21262d;
            border-radius: 4px;
        }
        QLabel#statsLabel {
            color: #8b949e;
            font-size: 11px;
            padding: 4px 12px;
            background: #161b22;
            border-top: 1px solid #21262d;
        }
        QProgressBar {
            background-color: #21262d;
            border: 1px solid #30363d;
            border-radius: 4px;
            text-align: center;
            color: #c9d1d9;
            height: 14px;
        }
        QProgressBar::chunk { background-color: #388bfd; border-radius: 3px; }
        QGroupBox {
            border: 1px solid #30363d;
            border-radius: 4px;
            margin-top: 8px;
            padding-top: 4px;
            color: #58a6ff;
            font-size: 10px;
            letter-spacing: 2px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 8px;
            padding: 0 4px;
        }
        QSplitter::handle { background-color: #21262d; width: 2px; height: 2px; }
        QScrollBar:vertical {
            background: #0d1117;
            width: 8px;
        }
        QScrollBar::handle:vertical {
            background: #30363d;
            border-radius: 4px;
            min-height: 20px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal {
            background: #0d1117;
            height: 8px;
        }
        QScrollBar::handle:horizontal {
            background: #30363d;
            border-radius: 4px;
            min-width: 20px;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
    )");

    // Build memory hierarchy with defaults
    rebuildMemoryHierarchy(m_assoc);

    // Central widget
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(8, 8, 8, 4);
    rootLayout->setSpacing(6);

    buildToolbar();
    rootLayout->addWidget(findChild<QWidget*>("toolbar"));

    // Tab widget holds all four views
    m_tabs = new QTabWidget(this);
    rootLayout->addWidget(m_tabs, 1);

    buildRegisterTable();
    buildDramTable();
    buildCacheTable();
    buildPipelineTable();
    buildStatsBar();

    rootLayout->addWidget(m_statsLabel);

    setWindowTitle("GIA Simulator");
    resize(1200, 750);
    refreshAll();
}

MainWindow::~MainWindow() {
    if (m_worker && m_worker->isRunning()) { m_worker->terminate(); m_worker->wait(); }
    delete m_dram;
    delete m_cache;
    delete m_regs;
}

// ─── Toolbar ──────────────────────────────────────────────────────────────────

void MainWindow::buildToolbar() {
    QWidget* bar = new QWidget(this);
    bar->setObjectName("toolbar");
    bar->setFixedHeight(48);
    QHBoxLayout* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    // File section
    m_loadBtn = new QPushButton("⬆  LOAD FILE", bar);
    m_fileLabel = new QLabel("No file loaded", bar);
    m_fileLabel->setObjectName("fileLabel");
    m_fileLabel->setMinimumWidth(220);

    // Control section
    m_stepBtn = new QPushButton("⏭  STEP", bar);
    m_stepBtn->setObjectName("stepBtn");
    m_runBtn  = new QPushButton("▶  RUN ALL", bar);
    m_runBtn->setObjectName("runBtn");
    m_resetBtn = new QPushButton("↺  RESET", bar);
    m_resetBtn->setObjectName("resetBtn");

    // Config section
    QLabel* assocLabel = new QLabel("N-Way:", bar);
    assocLabel->setStyleSheet("color:#8b949e; font-size:11px;");
    m_assocCombo = new QComboBox(bar);
    m_assocCombo->addItems({"1 (Direct)", "2-Way", "4-Way", "8-Way", "16-Way"});
    m_assocCombo->setCurrentIndex(2);
    m_assocCombo->setFixedWidth(110);

    QLabel* modeLabel = new QLabel("Mode:", bar);
    modeLabel->setStyleSheet("color:#8b949e; font-size:11px;");
    m_modeCombo = new QComboBox(bar);
    m_modeCombo->addItems({"All", "No Pipeline", "No Cache"});
    m_modeCombo->setCurrentIndex(0);
    m_modeCombo->setFixedWidth(110);
    m_modeCombo->setToolTip("All: full pipeline + cache\nNo Pipeline: instructions retire immediately\nNo Cache: reads/writes go straight to DRAM");

    QLabel* batchLabel = new QLabel("Batch:", bar);
    batchLabel->setStyleSheet("color:#8b949e; font-size:11px;");
    m_batchSpin = new QSpinBox(bar);
    m_batchSpin->setRange(1, 10000);
    m_batchSpin->setValue(1);
    m_batchSpin->setFixedWidth(70);
    m_batchSpin->setToolTip("Refresh UI every N instructions");

    m_progressBar = new QProgressBar(bar);
    m_progressBar->setRange(0, 100);
    m_progressBar->setVisible(false);

    m_cycleLabel = new QLabel("CYCLES: 0", bar);
    m_cycleLabel->setObjectName("cycleLabel");

    // Layout
    layout->addWidget(m_loadBtn);
    layout->addWidget(m_fileLabel);
    layout->addSpacing(8);

    // Separator
    QFrame* sep1 = new QFrame(bar);
    sep1->setFrameShape(QFrame::VLine);
    sep1->setStyleSheet("color:#30363d;");
    layout->addWidget(sep1);

    layout->addWidget(m_stepBtn);
    layout->addWidget(m_runBtn);
    layout->addWidget(m_resetBtn);

    QFrame* sep2 = new QFrame(bar);
    sep2->setFrameShape(QFrame::VLine);
    sep2->setStyleSheet("color:#30363d;");
    layout->addWidget(sep2);

    layout->addWidget(assocLabel);
    layout->addWidget(m_assocCombo);
    layout->addWidget(modeLabel);
    layout->addWidget(m_modeCombo);
    layout->addWidget(batchLabel);
    layout->addWidget(m_batchSpin);

    layout->addStretch();
    layout->addWidget(m_progressBar);
    layout->addWidget(m_cycleLabel);

    // Connections
    connect(m_loadBtn,   &QPushButton::clicked, this, &MainWindow::onLoadFile);
    connect(m_stepBtn,   &QPushButton::clicked, this, &MainWindow::onStepInstruction);
    connect(m_runBtn,    &QPushButton::clicked, this, &MainWindow::onRunAll);
    connect(m_resetBtn,  &QPushButton::clicked, this, &MainWindow::onReset);
    connect(m_assocCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onAssocChanged);
    connect(m_batchSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onBatchSizeChanged);
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &MainWindow::onModeChanged);
}

// ─── Register Table ───────────────────────────────────────────────────────────

void MainWindow::buildRegisterTable() {
    QWidget* container = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(12);

    // Integer registers (x0–x31)
    QGroupBox* intGroup = new QGroupBox("INTEGER REGISTERS  x0 – x31");
    QVBoxLayout* intLayout = new QVBoxLayout(intGroup);
    intLayout->setContentsMargins(4, 12, 4, 4);

    m_intRegTable = new QTableWidget(32, 4, intGroup);
    m_intRegTable->setHorizontalHeaderLabels({"Reg", "Alias", "Dec", "Hex"});
    m_intRegTable->setAlternatingRowColors(true);
    m_intRegTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_intRegTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_intRegTable->verticalHeader()->setVisible(false);
    m_intRegTable->horizontalHeader()->setStretchLastSection(true);
    m_intRegTable->setColumnWidth(0, 40);
    m_intRegTable->setColumnWidth(1, 55);
    m_intRegTable->setColumnWidth(2, 90);
    m_intRegTable->setColumnWidth(3, 90);
    m_intRegTable->setRowHeight(0, 22);
    for (int i = 0; i < 32; ++i) m_intRegTable->setRowHeight(i, 22);
    intLayout->addWidget(m_intRegTable);
    intGroup->setLayout(intLayout);

    // Float registers (f0–f15)
    QGroupBox* fltGroup = new QGroupBox("FLOAT REGISTERS  f0 – f15");
    QVBoxLayout* fltLayout = new QVBoxLayout(fltGroup);
    fltLayout->setContentsMargins(4, 12, 4, 4);

    m_floatRegTable = new QTableWidget(16, 3, fltGroup);
    m_floatRegTable->setHorizontalHeaderLabels({"Reg", "Value", "Hex Bits"});
    m_floatRegTable->setAlternatingRowColors(true);
    m_floatRegTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_floatRegTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_floatRegTable->verticalHeader()->setVisible(false);
    m_floatRegTable->horizontalHeader()->setStretchLastSection(true);
    m_floatRegTable->setColumnWidth(0, 40);
    m_floatRegTable->setColumnWidth(1, 100);
    for (int i = 0; i < 16; ++i) m_floatRegTable->setRowHeight(i, 22);
    fltLayout->addWidget(m_floatRegTable);
    fltGroup->setLayout(fltLayout);

    layout->addWidget(intGroup, 3);
    layout->addWidget(fltGroup, 2);

    m_tabs->addTab(container, "REGISTERS");
}

// ─── DRAM Table ───────────────────────────────────────────────────────────────

void MainWindow::buildDramTable() {
    QWidget* container = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);

    QGroupBox* group = new QGroupBox(
        QString("DRAM  — %1 lines × %2 words  (delay=%3)")
            .arg(DRAM_LINES).arg(WORDS_PER_LINE).arg(DRAM_DELAY));
    QVBoxLayout* gl = new QVBoxLayout(group);
    gl->setContentsMargins(4, 12, 4, 4);

    // Columns: Line | W0 | W1 | W2 | W3
    m_dramTable = new QTableWidget(0, 5, group);
    m_dramTable->setHorizontalHeaderLabels({"Line", "W[0]", "W[1]", "W[2]", "W[3]"});
    m_dramTable->setAlternatingRowColors(true);
    m_dramTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_dramTable->verticalHeader()->setVisible(false);
    m_dramTable->horizontalHeader()->setStretchLastSection(true);
    m_dramTable->setColumnWidth(0, 60);
    for (int c = 1; c <= 4; ++c) m_dramTable->setColumnWidth(c, 90);

    gl->addWidget(m_dramTable);
    group->setLayout(gl);
    layout->addWidget(group);
    m_tabs->addTab(container, "DRAM");
}

// ─── Cache Table ──────────────────────────────────────────────────────────────

void MainWindow::buildCacheTable() {
    QWidget* container = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);

    QGroupBox* group = new QGroupBox(
        QString("CACHE  — %1 lines, %2-way set associative  (delay=%3)")
            .arg(CACHE_LINES).arg(m_assoc).arg(CACHE_DELAY));
    QVBoxLayout* gl = new QVBoxLayout(group);
    gl->setContentsMargins(4, 12, 4, 4);

    // Columns: Set | Way | Valid | Tag | LRU | W0 | W1 | W2 | W3
    m_cacheTable = new QTableWidget(0, 9, group);
    m_cacheTable->setHorizontalHeaderLabels(
        {"Set", "Way", "Valid", "Tag", "LRU", "W[0]", "W[1]", "W[2]", "W[3]"});
    m_cacheTable->setAlternatingRowColors(true);
    m_cacheTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_cacheTable->verticalHeader()->setVisible(false);
    m_cacheTable->horizontalHeader()->setStretchLastSection(true);
    m_cacheTable->setColumnWidth(0, 40);
    m_cacheTable->setColumnWidth(1, 40);
    m_cacheTable->setColumnWidth(2, 45);
    m_cacheTable->setColumnWidth(3, 70);
    m_cacheTable->setColumnWidth(4, 40);
    for (int c = 5; c <= 8; ++c) m_cacheTable->setColumnWidth(c, 80);

    gl->addWidget(m_cacheTable);
    group->setLayout(gl);
    layout->addWidget(group);
    m_tabs->addTab(container, "CACHE");
}

// ─── Pipeline Table ───────────────────────────────────────────────────────────

void MainWindow::buildPipelineTable() {
    QWidget* container = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);

    QGroupBox* group = new QGroupBox("PIPELINE  — 5-Stage (Fetch → Decode → Execute → Memory → Writeback)");
    QVBoxLayout* gl = new QVBoxLayout(group);
    gl->setContentsMargins(4, 12, 4, 4);

    // One row per stage; columns: Stage | Status | PC | Raw Word | Notes
    m_pipelineTable = new QTableWidget(5, 5, group);
    m_pipelineTable->setHorizontalHeaderLabels({"Stage", "Status", "PC", "Raw Word", "Info"});
    m_pipelineTable->setAlternatingRowColors(false);
    m_pipelineTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_pipelineTable->verticalHeader()->setVisible(false);
    m_pipelineTable->horizontalHeader()->setStretchLastSection(true);
    m_pipelineTable->setColumnWidth(0, 100);
    m_pipelineTable->setColumnWidth(1, 80);
    m_pipelineTable->setColumnWidth(2, 70);
    m_pipelineTable->setColumnWidth(3, 100);

    // Preset stage names and colours
    const QStringList stages = {"FETCH","DECODE","EXECUTE","MEMORY","WRITEBACK"};
    const QStringList colors = {"#1a3a5c","#1a3a2a","#3a2a1a","#2a1a3a","#1a2a3a"};
    for (int r = 0; r < 5; ++r) {
        m_pipelineTable->setRowHeight(r, 40);
        auto* nameItem = new QTableWidgetItem(stages[r]);
        nameItem->setForeground(QColor("#58a6ff"));
        nameItem->setFont(QFont("Consolas", 11, QFont::Bold));
        m_pipelineTable->setItem(r, 0, nameItem);
        for (int c = 0; c < 5; ++c) {
            auto* it = m_pipelineTable->item(r, c);
            if (!it) { it = new QTableWidgetItem(); m_pipelineTable->setItem(r, c, it); }
            it->setBackground(QColor(colors[r]));
        }
    }

    gl->addWidget(m_pipelineTable);
    group->setLayout(gl);
    layout->addWidget(group);
    m_tabs->addTab(container, "PIPELINE");
}

// ─── Stats bar ────────────────────────────────────────────────────────────────

void MainWindow::buildStatsBar() {
    m_statsLabel = new QLabel(this);
    m_statsLabel->setObjectName("statsLabel");
    m_statsLabel->setText("  Cache hits: 0   Cache misses: 0   Miss rate: 0.00%   DRAM hits: 0   DRAM misses: 0");
}

// ─── Refresh helpers ──────────────────────────────────────────────────────────

void MainWindow::refreshAll() {
    refreshRegisters();
    refreshDram();
    refreshCache();
    refreshPipeline();
    refreshStats();
    m_cycleLabel->setText(QString("CYCLES: %1").arg(m_cmdCount));
}

void MainWindow::refreshRegisters() {
    if (!m_regs) return;

    // Integer registers
    static const char* aliases[] = {
        "zero","pc","sp","ra","t0","t1","t2","t3",
        "t4","t5","t6","t7","s0","s1","s2","s3",
        "s4","s5","s6","s7","a0","a1","a2","a3",
        "a4","a5","a6","a7","fp","gp","tp","--"
    };
    int32_t* ir = m_regs->getIntRegs();
    for (int i = 0; i < 32; ++i) {
        auto setCell = [&](int col, const QString& text, QColor fg = QColor("#c9d1d9")) {
            QTableWidgetItem* it = m_intRegTable->item(i, col);
            if (!it) { it = new QTableWidgetItem(); m_intRegTable->setItem(i, col, it); }
            it->setText(text);
            it->setForeground(fg);
        };
        setCell(0, QString("x%1").arg(i),   QColor("#58a6ff"));
        setCell(1, aliases[i < 31 ? i : 31],QColor("#8b949e"));
        setCell(2, QString::number(ir[i]),   ir[i] != 0 ? QColor("#e6edf3") : QColor("#484f58"));
        setCell(3, QString("0x%1").arg(static_cast<uint32_t>(ir[i]), 8, 16, QChar('0')).toUpper(),
                QColor("#79c0ff"));
    }

    // Float registers
    float* fr = m_regs->getFloatRegs();
    for (int i = 0; i < 16; ++i) {
        auto setFCell = [&](int col, const QString& text, QColor fg = QColor("#c9d1d9")) {
            QTableWidgetItem* it = m_floatRegTable->item(i, col);
            if (!it) { it = new QTableWidgetItem(); m_floatRegTable->setItem(i, col, it); }
            it->setText(text);
            it->setForeground(fg);
        };
        uint32_t bits; std::memcpy(&bits, &fr[i], 4);
        setFCell(0, QString("f%1").arg(i),  QColor("#58a6ff"));
        setFCell(1, QString::number(static_cast<double>(fr[i]), 'g', 7),
                 fr[i] != 0.f ? QColor("#e6edf3") : QColor("#484f58"));
        setFCell(2, QString("0x%1").arg(bits, 8, 16, QChar('0')).toUpper(), QColor("#ffa657"));
    }
}

void MainWindow::refreshDram() {
    if (!m_dram) return;

    uint32_t numLines = m_dram->getNumLines();
    m_dramTable->setRowCount(static_cast<int>(numLines));

    for (uint32_t line = 0; line < numLines; ++line) {
        const uint32_t* data = m_dram->peekLine(line);
        m_dramTable->setRowHeight(static_cast<int>(line), 22);

        bool anyNonZero = false;
        if (data) for (int w = 0; w < WORDS_PER_LINE; ++w) if (data[w]) { anyNonZero = true; break; }

        // Line index
        QTableWidgetItem* li = m_dramTable->item(line, 0);
        if (!li) { li = new QTableWidgetItem(); m_dramTable->setItem(line, 0, li); }
        li->setText(QString::number(line));
        li->setForeground(QColor("#8b949e"));

        for (int w = 0; w < WORDS_PER_LINE; ++w) {
            QTableWidgetItem* it = m_dramTable->item(line, w + 1);
            if (!it) { it = new QTableWidgetItem(); m_dramTable->setItem(line, w + 1, it); }
            uint32_t val = data ? data[w] : 0;
            it->setText(QString("0x%1").arg(val, 8, 16, QChar('0')).toUpper());
            it->setForeground(val != 0 ? QColor("#e6edf3") : QColor("#30363d"));
        }
    }
}

void MainWindow::refreshCache() {
    if (!m_cache) return;

    uint32_t numSets = m_cache->getNumLines() / m_assoc;
    int totalRows = static_cast<int>(numSets * m_assoc);
    m_cacheTable->setRowCount(totalRows);

    int row = 0;
    for (uint32_t s = 0; s < numSets; ++s) {
        for (uint32_t w = 0; w < m_assoc; ++w, ++row) {
            m_cacheTable->setRowHeight(row, 22);
            const CacheLine* cl = m_cache->peekCacheLine(s, w);

            bool valid = cl && cl->valid;
            QColor rowColor = valid ? QColor("#1a2a1a") : QColor("#0d1117");

            auto setCacheCell = [&](int col, const QString& text, QColor fg) {
                QTableWidgetItem* it = m_cacheTable->item(row, col);
                if (!it) { it = new QTableWidgetItem(); m_cacheTable->setItem(row, col, it); }
                it->setText(text);
                it->setForeground(fg);
                it->setBackground(rowColor);
            };

            setCacheCell(0, QString::number(s),         QColor("#8b949e"));
            setCacheCell(1, QString::number(w),         QColor("#484f58"));
            setCacheCell(2, valid ? "✓" : "–",          valid ? QColor("#3fb950") : QColor("#484f58"));
            setCacheCell(3, cl ? QString("0x%1").arg(cl->tag,4,16,QChar('0')).toUpper() : "–",
                         QColor("#ffa657"));
            setCacheCell(4, cl ? QString::number(cl->lruCounter) : "–", QColor("#8b949e"));

            for (int dw = 0; dw < WORDS_PER_LINE; ++dw) {
                uint32_t val = (cl && valid) ? cl->data[dw] : 0;
                setCacheCell(5 + dw,
                             (cl && valid) ? QString("0x%1").arg(val,8,16,QChar('0')).toUpper() : "–",
                             val != 0 ? QColor("#e6edf3") : QColor("#30363d"));
            }
        }
    }
}

void MainWindow::refreshPipeline() {
    // Stub — replace with real pipeline stage inspection once Pipeline is wired in.
    // Each stage reads from pipeline->fetInst, decInst, exInst, memInst, wbInst.
    const QStringList stages   = {"FETCH","DECODE","EXECUTE","MEMORY","WRITEBACK"};
    const QStringList statuses = {"IDLE","IDLE","IDLE","IDLE","IDLE"};

    for (int r = 0; r < 5; ++r) {
        auto setP = [&](int col, const QString& text, QColor fg = QColor("#c9d1d9")) {
            QTableWidgetItem* it = m_pipelineTable->item(r, col);
            if (!it) { it = new QTableWidgetItem(); m_pipelineTable->setItem(r, col, it); }
            it->setText(text);
            it->setForeground(fg);
        };
        setP(0, stages[r],   QColor("#58a6ff"));
        setP(1, statuses[r], QColor("#8b949e"));
        setP(2, "–",         QColor("#484f58"));
        setP(3, "–",         QColor("#484f58"));
        setP(4, "–",         QColor("#484f58"));
    }

    // ── When Pipeline is available, replace the stub above with something like: ──
    //
    // auto fillStage = [&](int row, const Instruction& inst, const QString& name) {
    //     setP(0, name,    QColor("#58a6ff"));
    //     setP(1, inst.valid ? "ACTIVE" : "BUBBLE",
    //          inst.valid ? QColor("#3fb950") : QColor("#484f58"));
    //     setP(2, inst.valid ? QString("0x%1").arg(inst.pc,4,16,QChar('0')).toUpper() : "–", QColor("#79c0ff"));
    //     setP(3, inst.valid ? QString("0x%1").arg(inst.raw,8,16,QChar('0')).toUpper() : "–", QColor("#e6edf3"));
    //     setP(4, inst.valid ? describeInst(inst) : "–", QColor("#8b949e"));
    // };
    // fillStage(0, pipeline->fetInst, "FETCH");
    // fillStage(1, pipeline->decInst, "DECODE");
    // fillStage(2, pipeline->exInst,  "EXECUTE");
    // fillStage(3, pipeline->memInst, "MEMORY");
    // fillStage(4, pipeline->wbInst,  "WRITEBACK");
}

void MainWindow::refreshStats() {
    if (!m_cache || !m_dram) return;
    m_statsLabel->setText(
        QString("  Cache hits: %1   Cache misses: %2   Miss rate: %3%"
                "   DRAM hits: %4   DRAM misses: %5   Total cycles: %6")
            .arg(m_cache->getHitCount())
            .arg(m_cache->getMissCount())
            .arg(static_cast<double>(m_cache->getMissRate() * 100.f), 0, 'f', 2)
            .arg(m_dram->getHitCount())
            .arg(m_dram->getMissCount())
            .arg(m_cmdCount));
}

// ─── Slots ────────────────────────────────────────────────────────────────────

void MainWindow::onLoadFile() {
    QString path = QFileDialog::getOpenFileName(
        this, "Load Hex Program", "",
        "Hex Files (*.hex *.txt);;All Files (*)");
    if (path.isEmpty()) return;
    m_loadedFile = path;
    m_fileLabel->setText(QFileInfo(path).fileName());
    loadHexFile(path);
    refreshAll();
}

void MainWindow::onStepInstruction() {
    executeSingleInstruction();
    ++m_sinceRefresh;
    if (m_sinceRefresh >= m_batchSize) {
        refreshAll();
        m_sinceRefresh = 0;
    }
}

void MainWindow::onRunAll() {
    if (m_loadedFile.isEmpty()) {
        QMessageBox::information(this, "No file", "Please load a .hex file first.");
        return;
    }

    // Disable controls during run
    m_stepBtn->setEnabled(false);
    m_runBtn->setEnabled(false);
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 1000);
    m_progressBar->setValue(0);

    m_worker = new SimWorker(m_cache, m_dram, m_regs, 1000, this);
    connect(m_worker, &SimWorker::progress, this, &MainWindow::onWorkerProgress);
    connect(m_worker, &SimWorker::finished, this, &MainWindow::onWorkerDone);
    m_worker->start();
}

void MainWindow::onReset() {
    if (m_cache) m_cache->reset();
    if (m_dram)  m_dram->reset();
    if (m_regs)  m_regs->reset();
    m_cmdCount     = 0;
    m_sinceRefresh = 0;
    m_fileLabel->setText("No file loaded");
    m_loadedFile.clear();
    refreshAll();
}

void MainWindow::onAssocChanged(int index) {
    static const uint32_t assocValues[] = {1, 2, 4, 8, 16};
    if (index < 0 || index > 4) return;
    m_assoc = assocValues[index];
    rebuildMemoryHierarchy(m_assoc);
    refreshAll();
}

void MainWindow::onModeChanged(int index) {
    switch (index) {
        //case 0: m_simMode = SimMode::ALL;         break;
        //case 1: m_simMode = SimMode::NO_PIPELINE; break;
        //case 2: m_simMode = SimMode::NO_CACHE;    break;
    }

    // Grey out N-Way selector when cache is disabled — it has no effect
    //bool cacheActive = (m_simMode != SimMode::NO_CACHE);
    //m_assocCombo->setEnabled(cacheActive);

    // TODO: pass m_simMode into your pipeline/memory calls
    // e.g. pipeline->setPipelineEnabled(m_simMode != SimMode::NO_PIPELINE);
    // e.g. pass &dram directly instead of &cache when NO_CACHE
}

void MainWindow::onWorkerProgress(int completed) {
    m_progressBar->setValue(completed);
    // Batch-throttled UI refresh during run
    //m_cmdCount += (completed - m_progressBar->value() + 1);
    refreshStats();
    m_cycleLabel->setText(QString("CYCLES: %1").arg(completed));
}

void MainWindow::onWorkerDone() {
    m_worker->deleteLater();
    m_worker = nullptr;
    m_progressBar->setVisible(false);
    m_stepBtn->setEnabled(true);
    m_runBtn->setEnabled(true);
    refreshAll();
}

void MainWindow::onBatchSizeChanged(int val) {
    m_batchSize = val;
}

// ─── Simulation helpers ───────────────────────────────────────────────────────

void MainWindow::executeSingleInstruction() {
    if (!m_cache || !m_regs) return;

    //if (m_simMode == SimMode::NO_PIPELINE) {
        // pipeline->setPipelineEnabled(false);
    //} else {
        // pipeline->setPipelineEnabled(true);
    //}

    // When NO_CACHE, bypass cache and go straight to DRAM:
    // Memory* activeMemory = (m_simMode == SimMode::NO_CACHE) ? m_dram : m_cache;
    // pipeline->tick();

    ++m_cmdCount;
}

void MainWindow::rebuildMemoryHierarchy(uint32_t assoc) {
    delete m_cache;
    delete m_dram;
    delete m_regs;

    m_dram  = new Memory(DRAM_LINES,  DRAM_DELAY,  nullptr,  1);
    m_cache = new Memory(CACHE_LINES, CACHE_DELAY, m_dram,   assoc);
    m_regs  = new RegisterFile();
    m_assoc = assoc;
}

void MainWindow::loadHexFile(const QString& path) {
    if (!m_dram) return;
    std::ifstream file(path.toStdString());
    if (!file) {
        QMessageBox::warning(this, "Load Error",
            QString("Could not open:\n%1").arg(path));
        return;
    }
    uint32_t address = 0;
    std::string token;
    while (file >> token) {
        uint32_t word = static_cast<uint32_t>(std::stoul(token, nullptr, 16));
        m_dram->writeWordDirect(address++, word);
    }
}
