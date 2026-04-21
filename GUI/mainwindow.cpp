#include "mainwindow.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QListWidget>
#include <QThread>
#include <QFileDialog>
#include <QFileInfo>
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

static QString formatWord(uint32_t value) {
    return QString("0x%1").arg(value, 8, 16, QChar('0')).toUpper();
}

static QString formatDecimal(uint32_t value) {
    return QString::number(value);
}

static QString instructionSummary(const Instruction& inst) {
    return QString::fromStdString(inst.toString());
}

// ─── MainWindow ───────────────────────────────────────────────

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
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
        QPushButton:hover   { background-color: #30363d; border-color: #58a6ff; color: #58a6ff; }
        QPushButton:pressed { background-color: #1f3a5f; }
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
        QPushButton#stopBtn {
            background-color: #3d1f1f;
            border-color: #f85149;
            color: #f85149;
            font-weight: bold;
        }
        QPushButton#stopBtn:hover { background-color: #f85149; color: #ffffff; }
        QPushButton#bpBtn {
            background-color: #2a1f3d;
            border-color: #8957e5;
            color: #a5a0ff;
            font-weight: bold;
        }
        QPushButton#bpBtn:hover { background-color: #8957e5; color: #ffffff; }
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
        QScrollBar:vertical { background: #30363d; width: 8px; }
        QScrollBar::handle:vertical {
            background: #c9d1d9;
            border-radius: 4px;
            min-height: 20px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal { background: #30363d; height: 8px; }
        QScrollBar::handle:horizontal {
            background: #c9d1d9;
            border-radius: 4px;
            min-width: 20px;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

        /* ── Stats bar individual labels ── */
        QLabel.statValue {
            color: #e6edf3;
            font-size: 13px;
            font-weight: bold;
            padding: 6px 14px;
            background: #161b22;
            border: 1px solid #21262d;
            border-radius: 4px;
            min-width: 40px;
        }
        QLabel.statKey {
            color: #8b949e;
            font-size: 11px;
            padding: 0 4px;
        }

        /* ── Checkboxes ── */
        QCheckBox { color: #c9d1d9; font-size: 11px; spacing: 5px; }
        QCheckBox::indicator {
            width: 15px; height: 15px;
            border: 1px solid #484f58; border-radius: 3px; background-color: #21262d;
        }
        QCheckBox::indicator:checked   { background-color: #2ea043; border-color: #3fb950; }
        QCheckBox::indicator:unchecked { background-color: #3d1f1f; border-color: #f85149; }
        QCheckBox::indicator:checked:hover   { background-color: #3fb950; }
        QCheckBox::indicator:unchecked:hover { background-color: #f85149; }
        QCheckBox:disabled { color: #484f58; }
        QCheckBox::indicator:disabled { background-color: #161b22; border-color: #21262d; }
    )");

    rebuildMemoryHierarchy(m_assoc);

    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(8, 8, 8, 4);
    rootLayout->setSpacing(6);

    buildToolbar();
    rootLayout->addWidget(findChild<QWidget*>("toolbar"));
    // Double-click a BP in the list to remove it individually
    connect(m_bpList, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem* item) {
                onRemoveBP(m_bpList->row(item));
            });

    m_tabs = new QTabWidget(this);
    rootLayout->addWidget(m_tabs, 1);

    // ── Two combined tabs ─────────────────────────────────────
    buildRegisterAndPipelineTab();
    buildMemoryTab();

    // ── Stats bar at the bottom ───────────────────────────────
    buildStatsBar();
    rootLayout->addWidget(findChild<QWidget*>("statsBar"));

    setWindowTitle("GIA Simulator");
    resize(1200, 750);
    refreshAll();
}

MainWindow::~MainWindow() {
    // If the user closes the window mid-run, pause the clock and
    // wait for the worker thread to exit before destroying objects.
    if (m_running && m_clock) {
        m_stopRequested.store(true);
        m_clock->pause();   // sets halted=true so run() loop exits
    }
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
    delete m_clock;
    delete m_pipeline;
    delete m_cache;
    delete m_dram;
    delete m_regs;
}

// ─── Toolbar ──────────────────────────────────────────────────

void MainWindow::buildToolbar() {
    QWidget* bar = new QWidget(this);
    bar->setObjectName("toolbar");
    bar->setFixedHeight(96);   // two rows
    QVBoxLayout* barLayout = new QVBoxLayout(bar);
    barLayout->setContentsMargins(0, 2, 0, 2);
    barLayout->setSpacing(4);

    // ── Row 1: all existing controls unchanged ────────────────
    QWidget* row1 = new QWidget(bar);
    QHBoxLayout* layout = new QHBoxLayout(row1);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    m_loadBtn  = new QPushButton("LOAD FILE", row1);
    m_fileLabel = new QLabel("No file loaded", row1);
    m_fileLabel->setObjectName("fileLabel");
    m_fileLabel->setMinimumWidth(80);
    m_fileLabel->setMaximumWidth(150);
    m_fileLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_fileLabel->setTextFormat(Qt::PlainText);
    m_fileLabel->setWordWrap(false);

    m_stepBtn  = new QPushButton("STEP",    row1); m_stepBtn->setObjectName("stepBtn");
    m_runBtn   = new QPushButton("RUN ALL", row1); m_runBtn->setObjectName("runBtn");
    m_resetBtn = new QPushButton("RESET",   row1); m_resetBtn->setObjectName("resetBtn");

    QLabel* assocLabel = new QLabel("N-Way:", row1);
    assocLabel->setStyleSheet("color:#8b949e; font-size:11px;");
    m_assocCombo = new QComboBox(row1);
    m_assocCombo->addItems({"1 (Direct)", "2-Way", "4-Way", "8-Way", "16-Way"});
    m_assocCombo->setCurrentIndex(2);
    m_assocCombo->setFixedWidth(110);

    QWidget* checkStack = new QWidget(row1);
    checkStack->setFixedHeight(40);
    QVBoxLayout* checkLayout = new QVBoxLayout(checkStack);
    checkLayout->setContentsMargins(0, 0, 0, 0);
    checkLayout->setSpacing(2);
    m_pipelineCheck = new QCheckBox("✓ Pipeline", checkStack);
    m_pipelineCheck->setChecked(true);
    m_pipelineCheck->setToolTip("Uncheck to bypass the pipeline");
    m_cacheCheck = new QCheckBox("✓ Cache", checkStack);
    m_cacheCheck->setChecked(true);
    m_cacheCheck->setToolTip("Uncheck to bypass the L1 cache");
    checkLayout->addWidget(m_pipelineCheck);
    checkLayout->addWidget(m_cacheCheck);

    QLabel* stepModeLabel = new QLabel("Step By:", row1);
    stepModeLabel->setStyleSheet("color:#8b949e; font-size:11px;");
    m_stepModeCombo = new QComboBox(row1);
    m_stepModeCombo->addItems({"Cycle", "Instruction"});
    m_stepModeCombo->setCurrentIndex(0);
    m_stepModeCombo->setFixedWidth(110);
    m_stepModeCombo->setToolTip("STEP advances by the selected unit");

    m_progressBar = new QProgressBar(row1);
    m_progressBar->setRange(0, 100);
    m_progressBar->setFixedWidth(80);
    m_progressBar->setVisible(false);

    m_pcLabel = new QLabel("PC: 0", row1);
    m_pcLabel->setObjectName("pcLabel");
    m_pcLabel->setFixedWidth(85);
    m_pcLabel->setAlignment(Qt::AlignCenter);
    m_pcLabel->setStyleSheet(
        "color: #ffa657; font-size: 11px; font-weight: bold;"
        "padding: 4px 6px; background: #161b22;"
        "border: 1px solid #21262d; border-radius: 4px;");

    m_cycleLabel = new QLabel("CYCLES: 0", row1);
    m_cycleLabel->setObjectName("cycleLabel");
    m_cycleLabel->setFixedWidth(100);
    m_cycleLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(m_loadBtn);
    layout->addWidget(m_fileLabel);
    layout->addSpacing(4);

    QFrame* sep1 = new QFrame(row1);
    sep1->setFrameShape(QFrame::VLine);
    sep1->setStyleSheet("color:#30363d;");
    layout->addWidget(sep1);

    layout->addWidget(m_stepBtn);
    layout->addWidget(m_runBtn);
    layout->addWidget(m_resetBtn);

    QFrame* sep2 = new QFrame(row1);
    sep2->setFrameShape(QFrame::VLine);
    sep2->setStyleSheet("color:#30363d;");
    layout->addWidget(sep2);

    layout->addWidget(assocLabel);
    layout->addWidget(m_assocCombo);

    QFrame* sep3 = new QFrame(row1);
    sep3->setFrameShape(QFrame::VLine);
    sep3->setStyleSheet("color:#30363d;");
    layout->addWidget(sep3);

    layout->addWidget(checkStack);

    QFrame* sep4 = new QFrame(row1);
    sep4->setFrameShape(QFrame::VLine);
    sep4->setStyleSheet("color:#30363d;");
    layout->addWidget(sep4);

    layout->addWidget(stepModeLabel);
    layout->addWidget(m_stepModeCombo);

    layout->addStretch();
    layout->addWidget(m_progressBar);
    layout->addWidget(m_pcLabel);
    layout->addWidget(m_cycleLabel);

    // ── Row 2: breakpoint panel ───────────────────────────────
    // Layout:  [BP list (scrollable)] [addr spinbox] [+ ADD BP] [CLEAR ALL]
    // The list shows all active breakpoints; each row has a small
    // remove button. ADD BP calls clock->addBreakpoint(). CLEAR ALL
    // calls clock->clearBreakpoints() and empties the list.
    QWidget* row2 = new QWidget(bar);
    QHBoxLayout* bpLayout = new QHBoxLayout(row2);
    bpLayout->setContentsMargins(0, 0, 0, 0);
    bpLayout->setSpacing(6);

    QLabel* bpSectionLabel = new QLabel("Breakpoints:", row2);
    bpSectionLabel->setStyleSheet("color:#8b949e; font-size:11px;");
    bpLayout->addWidget(bpSectionLabel);

    // Compact scrollable list of active breakpoints
    m_bpList = new QListWidget(row2);
    m_bpList->setFixedHeight(36);
    m_bpList->setFlow(QListWidget::LeftToRight);   // horizontal scroll
    m_bpList->setWrapping(false);
    m_bpList->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_bpList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_bpList->setStyleSheet(
        "QListWidget { background:#161b22; border:1px solid #21262d;"
        "  border-radius:4px; color:#c9d1d9; font-size:11px; }"
        "QListWidget::item { padding:2px 6px; border-right:1px solid #21262d; }"
        "QListWidget::item:selected { background:#1f3a5f; }");
    m_bpList->setToolTip("Active breakpoints — select one and press Remove, or Clear All");
    bpLayout->addWidget(m_bpList, 1);   // takes available width

    // Address spinbox
    m_bpSpin = new QSpinBox(row2);
    m_bpSpin->setRange(0, static_cast<int>(DRAM_LINES * WORDS_PER_LINE - 1));
    m_bpSpin->setValue(0);
    m_bpSpin->setFixedWidth(80);
    m_bpSpin->setToolTip("Word address to add as a breakpoint");
    bpLayout->addWidget(m_bpSpin);

    // Add BP button
    m_addBpBtn = new QPushButton("ADD BP", row2);
    m_addBpBtn->setObjectName("bpBtn");
    m_addBpBtn->setFixedWidth(80);
    m_addBpBtn->setToolTip("Add this address to the breakpoint list");
    bpLayout->addWidget(m_addBpBtn);

    // Clear all button
    m_clearBpBtn = new QPushButton("CLEAR", row2);
    m_clearBpBtn->setObjectName("resetBtn");   // reuse red style
    m_clearBpBtn->setFixedWidth(75);
    m_clearBpBtn->setToolTip("Remove all breakpoints");
    bpLayout->addWidget(m_clearBpBtn);

    barLayout->addWidget(row1);
    barLayout->addWidget(row2);

    // ── Connections ───────────────────────────────────────────
    connect(m_loadBtn,   &QPushButton::clicked, this, &MainWindow::onLoadFile);
    connect(m_stepBtn,   &QPushButton::clicked, this, &MainWindow::onStep);
    connect(m_runBtn,    &QPushButton::clicked, this, &MainWindow::onRunAll);
    connect(m_resetBtn,  &QPushButton::clicked, this, &MainWindow::onReset);
    connect(m_addBpBtn,  &QPushButton::clicked, this, &MainWindow::onAddBP);
    connect(m_clearBpBtn,&QPushButton::clicked, this, &MainWindow::onClearBPs);
    connect(m_assocCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onAssocChanged);
    connect(m_pipelineCheck, &QCheckBox::toggled, this, &MainWindow::onPipelineToggled);
    connect(m_cacheCheck,    &QCheckBox::toggled, this, &MainWindow::onCacheToggled);
    connect(m_stepModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                m_stepMode = (index == 1) ? StepMode::INSTRUCTION : StepMode::CYCLE;
            });
}

// ─── Tab 1: Registers + Pipeline ──────────────────────────────

void MainWindow::buildRegisterAndPipelineTab() {
    QWidget* container = new QWidget();
    QVBoxLayout* outerLayout = new QVBoxLayout(container);
    outerLayout->setContentsMargins(8, 8, 8, 8);
    outerLayout->setSpacing(8);

    QWidget* regRow = new QWidget();
    QHBoxLayout* regLayout = new QHBoxLayout(regRow);
    regLayout->setContentsMargins(0, 0, 0, 0);
    regLayout->setSpacing(12);

    QGroupBox* intGroup = new QGroupBox("INTEGER REGISTERS");
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
    for (int i = 0; i < 32; ++i) m_intRegTable->setRowHeight(i, 18);
    intLayout->addWidget(m_intRegTable);
    intGroup->setLayout(intLayout);

    QGroupBox* fltGroup = new QGroupBox("FLOAT POINT REGISTERS");
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
    for (int i = 0; i < 16; ++i) m_floatRegTable->setRowHeight(i, 18);
    fltLayout->addWidget(m_floatRegTable);
    fltGroup->setLayout(fltLayout);

    regLayout->addWidget(intGroup,  3);
    regLayout->addWidget(fltGroup,  2);

    QGroupBox* pipeGroup = new QGroupBox(
        "5-STAGE RISC PIPELINE");
    QVBoxLayout* pipeLayout = new QVBoxLayout(pipeGroup);
    pipeLayout->setContentsMargins(4, 12, 4, 4);

    m_pipelineTable = new QTableWidget(5, 5, pipeGroup);
    m_pipelineTable->setHorizontalHeaderLabels(
        {"Stage", "Status", "Info", "Raw Word", "PC"});
    m_pipelineTable->setAlternatingRowColors(false);
    m_pipelineTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_pipelineTable->verticalHeader()->setVisible(false);
    // m_pipelineTable->horizontalHeader()->setStretchLastSection(true);
    // m_pipelineTable->setColumnWidth(0, 100);
    // m_pipelineTable->setColumnWidth(1, 80);
    // m_pipelineTable->setColumnWidth(2, 70);
    // m_pipelineTable->setColumnWidth(3, 100);

    m_pipelineTable->horizontalHeader()->setStretchLastSection(false);

    m_pipelineTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    m_pipelineTable->horizontalHeader()->setStretchLastSection(false);
    auto* header = m_pipelineTable->horizontalHeader();

    header->setSectionResizeMode(0, QHeaderView::Stretch); // Stage
    header->setSectionResizeMode(1, QHeaderView::Stretch); // Status
    header->setSectionResizeMode(2, QHeaderView::Stretch); // PC
    header->setSectionResizeMode(3, QHeaderView::Stretch); // Raw Word
    header->setSectionResizeMode(4, QHeaderView::Stretch); // Info

    // Assign the weights
    m_pipelineTable->horizontalHeader()->setStretchLastSection(false);
    m_pipelineTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // Apply specific weights
    header->setSortIndicatorClearable(false); // unrelated, but keeps header clean
    m_pipelineTable->horizontalHeader()->setDefaultSectionSize(150); // a baseline

    // To set actual 'Stretch Factors' in a QTableWidget header:
    header->setSectionResizeMode(0, QHeaderView::Stretch);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    header->setSectionResizeMode(2, QHeaderView::Stretch);
    header->setSectionResizeMode(3, QHeaderView::Stretch);
    header->setSectionResizeMode(4, QHeaderView::Stretch);

    const QStringList stages = {"FETCH","DECODE","EXECUTE","MEMORY","WRITEBACK"};
    const QStringList colors = {"#1a3a5c","#1a3a2a","#3a2a1a","#2a1a3a","#1a2a3a"};
    for (int r = 0; r < 5; ++r) {
        m_pipelineTable->setRowHeight(r, 36);
        auto* nameItem = new QTableWidgetItem(stages[r]);
        nameItem->setForeground(QColor("#58a6ff"));
        nameItem->setFont(QFont("Consolas", 12, QFont::Bold));
        m_pipelineTable->setItem(r, 0, nameItem);
        for (int c = 0; c < 5; ++c) {
            auto* it = m_pipelineTable->item(r, c);
            if (!it) { it = new QTableWidgetItem(); m_pipelineTable->setItem(r, c, it); }
            if (c == 0) {
                // Keep Stage Names at 11pt Bold (as originally styled)
                it->setFont(QFont("Consolas", 12, QFont::Bold));
            } else {
                // Increase font size for Status, PC, Raw, and Info
                // Change '14' to whatever size you prefer
                it->setFont(QFont("Consolas", 12));
            }
            it->setBackground(QColor(colors[r]));
        }
    }
    m_pipelineTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pipeLayout->addWidget(m_pipelineTable);
    pipeGroup->setLayout(pipeLayout);
    // Guarantee all 5 rows are always fully visible:
    // 5 rows × 36px = 180
    // + 26px  column header
    // + 8px   QGroupBox margin-top (from stylesheet)
    // + 4px   QGroupBox padding-top (from stylesheet)
    // + 12px  pipeLayout top contentsMargin
    // + 4px   pipeLayout bottom contentsMargin
    // = 234px minimum to prevent the last row being clipped
    pipeGroup->setMinimumHeight(262);

    outerLayout->addWidget(regRow,    2);
    outerLayout->addWidget(pipeGroup, 1);

    m_tabs->addTab(container, "CPU");
}

// ─── Tab 2: Memory (DRAM paginated left | Cache right) ──────────

void MainWindow::buildMemoryTab() {
    QWidget* container = new QWidget();
    QHBoxLayout* outerLayout = new QHBoxLayout(container);
    outerLayout->setContentsMargins(8, 8, 8, 8);
    outerLayout->setSpacing(0);

    QSplitter* splitter = new QSplitter(Qt::Horizontal, container);
    splitter->setHandleWidth(4);

    // ── DRAM panel (paginated) ────────────────────────────────
    QWidget* dramWidget = new QWidget();
    QVBoxLayout* dramOuterLayout = new QVBoxLayout(dramWidget);
    dramOuterLayout->setContentsMargins(0, 0, 4, 0);
    dramOuterLayout->setSpacing(0);

    QGroupBox* dramGroup = new QGroupBox(
        QString("DRAM  — %1 lines × %2 words  (delay=%3)  —  %4 lines/page")
            .arg(DRAM_LINES).arg(WORDS_PER_LINE)
            .arg(DRAM_DELAY).arg(DRAM_LINES_PER_PAGE));
    QVBoxLayout* dgl = new QVBoxLayout(dramGroup);
    dgl->setContentsMargins(4, 12, 4, 4);
    dgl->setSpacing(4);

    m_dramTable = new QTableWidget(static_cast<int>(DRAM_LINES_PER_PAGE), 5, dramGroup);
    m_dramTable->setHorizontalHeaderLabels({"Line","W[0]","W[1]","W[2]","W[3]"});
    m_dramTable->setAlternatingRowColors(true);
    m_dramTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_dramTable->verticalHeader()->setVisible(false);
    m_dramTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_dramTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_dramTable->setColumnWidth(0, 55);
    for (int r = 0; r < static_cast<int>(DRAM_LINES_PER_PAGE); ++r)
        m_dramTable->setRowHeight(r, 22);

    QWidget* pagerRow = new QWidget();
    QHBoxLayout* pagerLayout = new QHBoxLayout(pagerRow);
    pagerLayout->setContentsMargins(0, 2, 0, 0);
    pagerLayout->setSpacing(8);

    m_dramPrevBtn = new QPushButton("◀  Prev", pagerRow);
    m_dramPrevBtn->setFixedWidth(90);
    m_dramPrevBtn->setEnabled(false);

    m_dramPageLabel = new QLabel("Page 1 / 256  (lines 0 – 31)", pagerRow);
    m_dramPageLabel->setAlignment(Qt::AlignCenter);
    m_dramPageLabel->setStyleSheet(
        "color:#8b949e; font-size:11px; padding:4px 8px;"
        "background:#161b22; border:1px solid #21262d; border-radius:4px;");

    m_dramNextBtn = new QPushButton("Next  ▶", pagerRow);
    m_dramNextBtn->setFixedWidth(90);

    pagerLayout->addWidget(m_dramPrevBtn);
    pagerLayout->addStretch();
    pagerLayout->addWidget(m_dramPageLabel);
    pagerLayout->addStretch();
    pagerLayout->addWidget(m_dramNextBtn);

    connect(m_dramPrevBtn, &QPushButton::clicked, this, &MainWindow::onDramPagePrev);
    connect(m_dramNextBtn, &QPushButton::clicked, this, &MainWindow::onDramPageNext);

    dgl->addWidget(m_dramTable);
    dgl->addWidget(pagerRow);
    dramGroup->setLayout(dgl);
    dramOuterLayout->addWidget(dramGroup);
    splitter->addWidget(dramWidget);

    // ── Cache panel ───────────────────────────────────────────
    QWidget* cacheWidget = new QWidget();
    QVBoxLayout* cacheLayout = new QVBoxLayout(cacheWidget);
    cacheLayout->setContentsMargins(4, 0, 0, 0);

    QGroupBox* cacheGroup = new QGroupBox(
        QString("CACHE  — %1 sets, %2-way  (delay=%3)")
            .arg(CACHE_LINES).arg(m_assoc).arg(CACHE_DELAY));
    QVBoxLayout* cgl = new QVBoxLayout(cacheGroup);
    cgl->setContentsMargins(4, 12, 4, 4);

    m_cacheTable = new QTableWidget(0, 9, cacheGroup);
    m_cacheTable->setHorizontalHeaderLabels(
        {"Set","Way","Valid","Tag","LRU","W[0]","W[1]","W[2]","W[3]"});
    m_cacheTable->setAlternatingRowColors(true);
    m_cacheTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_cacheTable->verticalHeader()->setVisible(false);
    m_cacheTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_cacheTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_cacheTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_cacheTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_cacheTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_cacheTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_cacheTable->setColumnWidth(0, 38);
    m_cacheTable->setColumnWidth(1, 38);
    m_cacheTable->setColumnWidth(2, 42);
    m_cacheTable->setColumnWidth(3, 55);
    m_cacheTable->setColumnWidth(4, 38);

    cgl->addWidget(m_cacheTable);
    cacheGroup->setLayout(cgl);
    cacheLayout->addWidget(cacheGroup);
    splitter->addWidget(cacheWidget);

    splitter->setStretchFactor(0, 11);
    splitter->setStretchFactor(1, 9);

    outerLayout->addWidget(splitter);
    m_tabs->addTab(container, "MEMORY");
}

// ─── Stats bar (Restored original style) ──────────────────────

void MainWindow::buildStatsBar() {
    QWidget* bar = new QWidget(this);
    bar->setObjectName("statsBar");
    bar->setFixedHeight(58);
    bar->setStyleSheet("background: #161b22; border-top: 1px solid #21262d;");

    QHBoxLayout* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(12, 4, 12, 4);
    layout->setSpacing(0);

    auto makeStat = [&](const QString& key, QLabel*& valueOut) -> QWidget* {
        QWidget* cell = new QWidget();
        QVBoxLayout* vl = new QVBoxLayout(cell);
        vl->setContentsMargins(10, 2, 10, 2);
        vl->setSpacing(1);

        QLabel* keyLabel = new QLabel(key);
        keyLabel->setStyleSheet(
            "color: #8b949e; font-size: 10px; letter-spacing: 1px;");
        keyLabel->setAlignment(Qt::AlignHCenter);

        valueOut = new QLabel("0");
        valueOut->setStyleSheet(
            "color: #e6edf3; font-size: 15px; font-weight: bold;");
        valueOut->setAlignment(Qt::AlignHCenter);

        vl->addWidget(keyLabel);
        vl->addWidget(valueOut);
        return cell;
    };

    auto makeSep = [&]() -> QFrame* {
        QFrame* f = new QFrame();
        f->setFrameShape(QFrame::VLine);
        f->setStyleSheet("color: #21262d;");
        f->setFixedWidth(1);
        return f;
    };

    layout->addWidget(makeStat("CACHE HITS",    m_statCacheHits));
    layout->addWidget(makeSep());
    layout->addWidget(makeStat("CACHE MISSES",  m_statCacheMisses));
    layout->addWidget(makeSep());

    QWidget* mrCell = makeStat("MISS RATE",     m_statMissRate);
    layout->addWidget(mrCell);
    layout->addWidget(makeSep());

    layout->addWidget(makeStat("DRAM HITS",     m_statDRAMHits));
    layout->addWidget(makeSep());
    layout->addWidget(makeStat("DRAM MISSES",   m_statDRAMMisses));
    layout->addWidget(makeSep());
    layout->addWidget(makeStat("CYCLES",        m_statCycles));
    layout->addWidget(makeSep());
    layout->addWidget(makeStat("RETIRED",       m_statRetired));

    layout->addStretch();
}

// ─── Refresh helpers ──────────────────────────────────────────

void MainWindow::refreshAll() {
    refreshRegisters();
    refreshDram();
    refreshCache();
    refreshPipeline();
    refreshStats();

    uint32_t cycles = m_clock ? m_clock->getCycleCount() : 0;
    m_cycleLabel->setText(QString("CYCLES: %1").arg(cycles));

    int32_t pc = m_regs ? m_regs->readPC() : 0;
    m_pcLabel->setText(QString("PC: %1").arg(pc));
}

void MainWindow::refreshRegisters() {
    if (!m_regs) return;

    static const char* aliases[] = {
        "zero","pc","sp","ra","t0","t1","t2","t3",
        "t4","t5","t6","t7","s0","s1","s2","s3",
        "s4","s5","s6","s7","a0","a1","a2","a3",
        "a4","a5","a6","a7","fp","gp","tp","--"
    };
    int32_t* ir = m_regs->getIntRegs();
    for (int i = 0; i < 32; ++i) {
        auto setCell = [&](int col, const QString& text,
                           QColor fg = QColor("#c9d1d9")) {
            QTableWidgetItem* it = m_intRegTable->item(i, col);
            if (!it) { it = new QTableWidgetItem(); m_intRegTable->setItem(i, col, it); }
            it->setText(text);
            it->setForeground(fg);
        };
        setCell(0, QString("x%1").arg(i),     QColor("#58a6ff"));
        setCell(1, aliases[i < 31 ? i : 31],  QColor("#8b949e"));
        setCell(2, QString::number(ir[i]),
                ir[i] != 0 ? QColor("#e6edf3") : QColor("#484f58"));
        setCell(3, QString("0x%1")
                .arg(static_cast<uint32_t>(ir[i]), 8, 16, QChar('0')).toUpper(),
                QColor("#79c0ff"));
    }

    float* fr = m_regs->getFloatRegs();
    for (int i = 0; i < 16; ++i) {
        auto setFCell = [&](int col, const QString& text,
                            QColor fg = QColor("#c9d1d9")) {
            QTableWidgetItem* it = m_floatRegTable->item(i, col);
            if (!it) { it = new QTableWidgetItem(); m_floatRegTable->setItem(i, col, it); }
            it->setText(text);
            it->setForeground(fg);
        };
        uint32_t bits; std::memcpy(&bits, &fr[i], 4);
        setFCell(0, QString("f%1").arg(i), QColor("#58a6ff"));
        setFCell(1, QString::number(static_cast<double>(fr[i]), 'g', 7),
                 fr[i] != 0.f ? QColor("#e6edf3") : QColor("#484f58"));
        setFCell(2, QString("0x%1").arg(bits, 8, 16, QChar('0')).toUpper(),
                 QColor("#ffa657"));
    }
}

void MainWindow::refreshDram() {
    if (!m_dram) return;

    uint32_t totalLines = m_dram->getNumLines();
    uint32_t firstLine  = m_dramPage * DRAM_LINES_PER_PAGE;
    uint32_t lastLine   = qMin(firstLine + DRAM_LINES_PER_PAGE, totalLines);

    for (uint32_t line = firstLine; line < lastLine; ++line) {
        int row = static_cast<int>(line - firstLine);
        const uint32_t* data = m_dram->peekLine(line);

        QTableWidgetItem* li = m_dramTable->item(row, 0);
        if (!li) { li = new QTableWidgetItem(); m_dramTable->setItem(row, 0, li); }
        li->setText(QString::number(line));
        li->setForeground(QColor("#8b949e"));

        for (int w = 0; w < WORDS_PER_LINE; ++w) {
            QTableWidgetItem* it = m_dramTable->item(row, w + 1);
            if (!it) { it = new QTableWidgetItem(); m_dramTable->setItem(row, w + 1, it); }
            uint32_t val = data ? data[w] : 0;
            it->setText(QString::number(val));
            it->setForeground(val != 0 ? QColor("#e6edf3") : QColor("#30363d"));
        }
    }
    updateDramPageLabel();
}

void MainWindow::refreshCache() {
    if (!m_cache) return;

    uint32_t numSets  = m_cache->getNumLines() / m_assoc;
    int      totalRows = static_cast<int>(numSets * m_assoc);
    m_cacheTable->setRowCount(totalRows);

    int row = 0;
    for (uint32_t s = 0; s < numSets; ++s) {
        for (uint32_t w = 0; w < m_assoc; ++w, ++row) {
            m_cacheTable->setRowHeight(row, 22);
            const CacheLine* cl = m_cache->peekCacheLine(s, w);
            bool valid   = cl && cl->valid;
            QColor rowColor = valid ? QColor("#1a2a1a") : QColor("#0d1117");

            auto setCacheCell = [&](int col, const QString& text, QColor fg) {
                QTableWidgetItem* it = m_cacheTable->item(row, col);
                if (!it) { it = new QTableWidgetItem(); m_cacheTable->setItem(row, col, it); }
                it->setText(text);
                it->setForeground(fg);
                it->setBackground(rowColor);
            };

            setCacheCell(0, QString::number(s),  QColor("#8b949e"));
            setCacheCell(1, QString::number(w),  QColor("#484f58"));
            setCacheCell(2, valid ? "✓" : "–",   valid ? QColor("#3fb950") : QColor("#484f58"));
            setCacheCell(3, cl ? QString::number(cl->tag) : "–", QColor("#ffa657"));
            setCacheCell(4, cl ? QString::number(cl->lruCounter) : "–", QColor("#8b949e"));

            for (int dw = 0; dw < WORDS_PER_LINE; ++dw) {
                uint32_t val = (cl && valid) ? cl->data[dw] : 0;
                setCacheCell(5 + dw,
                             (cl && valid) ? QString::number(val) : "–",
                             val != 0 ? QColor("#e6edf3") : QColor("#30363d"));
            }
        }
    }
}

void MainWindow::refreshPipeline() {
    const QStringList stages = {"FETCH","DECODE","EXECUTE","MEMORY","WRITEBACK"};
    if (!m_pipeline) {
        for (int r = 0; r < 5; ++r) {
            for (int c = 1; c < 5; ++c) {
                QTableWidgetItem* it = m_pipelineTable->item(r, c);
                if (!it) { it = new QTableWidgetItem(); m_pipelineTable->setItem(r, c, it); }
                it->setText(c == 1 ? "IDLE" : "-");
                it->setForeground(QColor("#484f58"));
            }
        }
        return;
    }

    auto fillStage = [&](int row, const QString& name,
                         const Instruction& inst, bool stalled) {
        auto setP = [&](int col, const QString& text,
                        QColor fg = QColor("#c9d1d9")) {
            QTableWidgetItem* it = m_pipelineTable->item(row, col);
            if (!it) { it = new QTableWidgetItem(); m_pipelineTable->setItem(row, col, it); }
            it->setText(text);
            it->setForeground(fg);
        };

        bool    present = inst.squashed || (inst.valid && (row != 0 || inst.fetched));
        QString status  = "BUBBLE";
        QColor  statusColor("#484f58");
        if (stalled)         { status = "STALL";  statusColor = QColor("#f0883e"); }
        else if (inst.squashed){ status = "SQUASH"; statusColor = QColor("#ff7b72"); }
        else if (present)    { status = "ACTIVE"; statusColor = QColor("#3fb950"); }

        setP(0, name, QColor("#58a6ff"));
        setP(1, status, statusColor);
        setP(2, present ? instructionSummary(inst) : "bubble", QColor("#8b949e"));
        setP(3, present ? formatWord(inst.raw)      : "-",      QColor("#e6edf3"));
        setP(4, present ? formatDecimal(inst.pc)    : "-",      QColor("#79c0ff"));
    };

    fillStage(0, stages[0], m_pipeline->getFetchInst(),     m_pipeline->isFetchStalled());
    fillStage(1, stages[1], m_pipeline->getDecodeInst(),    m_pipeline->isDecodeStalled());
    fillStage(2, stages[2], m_pipeline->getExecuteInst(),   m_pipeline->isExecuteStalled());
    fillStage(3, stages[3], m_pipeline->getMemoryInst(),    m_pipeline->isMemoryStalled());
    fillStage(4, stages[4], m_pipeline->getWritebackInst(), m_pipeline->isWritebackStalled());
}

void MainWindow::refreshStats() {
    if (!m_cache || !m_dram || !m_clock) return;

    float missRate = m_cache->getMissRate() * 100.f;

    m_statCacheHits  ->setText(QString::number(m_cache->getHitCount()));
    m_statCacheMisses->setText(QString::number(m_cache->getMissCount()));
    m_statMissRate   ->setText(QString::number(static_cast<double>(missRate), 'f', 2) + "%");
    m_statDRAMHits   ->setText(QString::number(m_dram->getHitCount()));
    m_statDRAMMisses ->setText(QString::number(m_dram->getMissCount()));
    m_statCycles     ->setText(QString::number(m_clock->getCycleCount()));
    m_statRetired    ->setText(QString::number(m_clock->getInstrCount()));

    m_statMissRate->setStyleSheet(
        missRate > 0.f
        ? "color: #f0883e; font-size: 15px; font-weight: bold;"
        : "color: #3fb950; font-size: 15px; font-weight: bold;");
}

// ─── Slots ────────────────────────────────────────────────────

void MainWindow::onLoadFile() {
    QString path = QFileDialog::getOpenFileName(
        this, "Load Hex Program", "",
        "Hex Files (*.hex *.txt);;All Files (*)");
    if (path.isEmpty()) return;
    m_loadedFile = path;
    m_fileLabel->setText(QFileInfo(path).fileName());
    resetSimulationState();
    loadHexFile(path);
    refreshAll();
}

void MainWindow::onStep() {
    if (m_loadedFile.isEmpty()) {
        QMessageBox::information(this, "No file", "Please load a .hex file first.");
        return;
    }
    executeStep();
    refreshAll();
}

void MainWindow::onRunAll() {
    // ── If already running: request a stop ───────────────────
    // Clock::pause() sets halted=true with HaltReason::USER_PAUSE.
    // The run() loop checks halted at the top of every iteration
    // so it exits cleanly at the end of the current cycle.
    // The worker thread then emits runFinished() which calls
    // onRunFinished() on the GUI thread to restore all controls.
    if (m_running) {
        m_stopRequested.store(true);
        if (m_clock) m_clock->pause();   // uses existing Clock::pause()
        return;
    }

    // ── Start a new run ───────────────────────────────────────
    if (m_loadedFile.isEmpty()) {
        QMessageBox::information(this, "No file", "Please load a .hex file first.");
        return;
    }
    if (!m_clock || !m_pipeline) return;

    m_stopRequested.store(false);
    applySimulationMode();
    m_clock->resume();
    enterRunningState();

    // Run Clock::run() on a worker thread so the GUI stays live.
    // A lambda captures this; the signal/slot crossing back to the
    // GUI thread ensures refreshAll() runs on the correct thread.
    m_workerThread = QThread::create([this]() {
        m_clock->run();
        emit runFinished();
    });
    connect(m_workerThread, &QThread::finished,
            m_workerThread, &QObject::deleteLater);
    connect(this, &MainWindow::runFinished,
            this,  &MainWindow::onRunFinished,
            Qt::QueuedConnection);   // forces call onto GUI thread
    m_workerThread->start();
}

void MainWindow::onRunFinished() {
    // Called on the GUI thread after the worker thread exits.
    // Do NOT call this directly — it is only connected via signal.
    m_stopRequested.store(false);
    exitRunningState();
    refreshAll();
}

void MainWindow::enterRunningState() {
    m_running = true;
    // Repurpose the run button as a stop button
    m_runBtn->setText("STOP");
    m_runBtn->setObjectName("stopBtn");
    // Re-apply stylesheet so the new objectName takes effect
    m_runBtn->setStyleSheet("");   // force Qt to re-evaluate #stopBtn rule
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0); // indeterminate spinner
    // Disable everything except the stop button itself
    if (m_stepBtn)       m_stepBtn->setEnabled(false);
    if (m_resetBtn)      m_resetBtn->setEnabled(false);
    if (m_loadBtn)       m_loadBtn->setEnabled(false);
    if (m_addBpBtn)      m_addBpBtn->setEnabled(false);
    if (m_clearBpBtn)    m_clearBpBtn->setEnabled(false);
    if (m_bpSpin)        m_bpSpin->setEnabled(false);
    if (m_bpList)        m_bpList->setEnabled(false);
    if (m_assocCombo)    m_assocCombo->setEnabled(false);
    if (m_pipelineCheck) m_pipelineCheck->setEnabled(false);
    if (m_cacheCheck)    m_cacheCheck->setEnabled(false);
    if (m_stepModeCombo) m_stepModeCombo->setEnabled(false);
}

void MainWindow::exitRunningState() {
    m_running = false;
    // Restore the button to its original RUN ALL appearance
    m_runBtn->setObjectName("runBtn");
    m_runBtn->setText("RUN ALL");
    m_runBtn->setStyleSheet("");   // force Qt to re-evaluate #runBtn rule
    m_progressBar->setVisible(false);
    // Re-enable all controls
    if (m_stepBtn)       m_stepBtn->setEnabled(true);
    if (m_runBtn)        m_runBtn->setEnabled(true);
    if (m_resetBtn)      m_resetBtn->setEnabled(true);
    if (m_loadBtn)       m_loadBtn->setEnabled(true);
    if (m_addBpBtn)      m_addBpBtn->setEnabled(true);
    if (m_clearBpBtn)    m_clearBpBtn->setEnabled(true);
    if (m_bpSpin)        m_bpSpin->setEnabled(true);
    if (m_bpList)        m_bpList->setEnabled(true);
    if (m_assocCombo)    m_assocCombo->setEnabled(m_cacheEnabled);
    if (m_pipelineCheck) m_pipelineCheck->setEnabled(true);
    if (m_cacheCheck)    m_cacheCheck->setEnabled(true);
    if (m_stepModeCombo) m_stepModeCombo->setEnabled(true);
}

void MainWindow::onReset() {
    resetSimulationState();
    if (!m_loadedFile.isEmpty()) {
        loadHexFile(m_loadedFile);
        m_fileLabel->setText(QFileInfo(m_loadedFile).fileName());
    } else {
        m_fileLabel->setText("No file loaded");
    }
    refreshAll();
}

void MainWindow::onAssocChanged(int index) {
    static const uint32_t assocValues[] = {1, 2, 4, 8, 16};
    if (index < 0 || index > 4) return;
    m_assoc = assocValues[index];
    rebuildMemoryHierarchy(m_assoc);
    refreshAll();
}

void MainWindow::onPipelineToggled(bool checked) {
    m_pipelineEnabled = checked;
    m_pipelineCheck->setText(checked ? "✓ Pipeline" : "✗ Pipeline");
    applySimulationMode();
    refreshAll();
}

void MainWindow::onCacheToggled(bool checked) {
    m_cacheEnabled = checked;
    m_cacheCheck->setText(checked ? "✓ Cache" : "✗ Cache");
    if (m_assocCombo) m_assocCombo->setEnabled(checked);
    applySimulationMode();
    refreshAll();
}

void MainWindow::onDramPagePrev() {
    if (m_dramPage == 0) return;
    --m_dramPage;
    refreshDram();
}

void MainWindow::onDramPageNext() {
    if (!m_dram) return;
    uint32_t totalPages =
        (m_dram->getNumLines() + DRAM_LINES_PER_PAGE - 1) / DRAM_LINES_PER_PAGE;
    if (m_dramPage + 1 >= totalPages) return;
    ++m_dramPage;
    refreshDram();
}

void MainWindow::onAddBP() {
    if (!m_clock) return;
    uint32_t addr = static_cast<uint32_t>(m_bpSpin->value());

    // Prevent duplicate entries in both the clock and the list
    if (m_clock->isBreakpoint(addr)) return;

    m_clock->addBreakpoint(addr);

    // Add a display row — shows the address. Clicking the row
    // and pressing Remove will delete it via onRemoveBP().
    // Display as plain decimal — shorter and matches the spinbox value directly
    QString label = QString::number(addr);
    QListWidgetItem* item = new QListWidgetItem(label, m_bpList);
    item->setData(Qt::UserRole, addr);   // store raw address for removal
    item->setForeground(QColor("#a5a0ff"));
}

void MainWindow::onClearBPs() {
    if (!m_clock) return;
    m_clock->clearBreakpoints();
    m_bpList->clear();
}

void MainWindow::onRemoveBP(int row) {
    if (!m_clock || row < 0 || row >= m_bpList->count()) return;
    QListWidgetItem* item = m_bpList->item(row);
    uint32_t addr = static_cast<uint32_t>(item->data(Qt::UserRole).toUInt());
    m_clock->removeBreakpoint(addr);
    delete m_bpList->takeItem(row);
}

// ─── Simulation helpers ───────────────────────────────────────

void MainWindow::executeStep() {
    if (!m_clock || !m_pipeline) return;
    applySimulationMode();
    m_clock->resume();
    if (m_stepMode == StepMode::INSTRUCTION)
        m_clock->runInstructions(1);
    else
        m_clock->runCycles(1);
}

void MainWindow::rebuildMemoryHierarchy(uint32_t assoc) {
    delete m_clock;
    delete m_pipeline;
    delete m_cache;
    delete m_dram;
    delete m_regs;

    m_dram     = new Memory(DRAM_LINES,  DRAM_DELAY,  nullptr, 1);
    m_cache    = new Memory(CACHE_LINES, CACHE_DELAY, m_dram,  assoc);
    m_regs     = new RegisterFile();
    m_pipeline = new Pipeline(m_cache, m_regs);
    m_clock    = new Clock();
    m_pipeline->setClock(m_clock);
    m_clock->setPipeline(m_pipeline);
    applySimulationMode();
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
    while (file >> token)
        m_dram->writeWordDirect(address++,
            static_cast<uint32_t>(std::stoul(token, nullptr, 16)));
}

void MainWindow::applySimulationMode() {
    if (m_pipeline) m_pipeline->setPipelineEnabled(m_pipelineEnabled);
    if (m_cache)    m_cache->setCacheEnabled(m_cacheEnabled);
    if (m_assocCombo) m_assocCombo->setEnabled(m_cacheEnabled);
}

void MainWindow::updateDramPageLabel() {
    if (!m_dram || !m_dramPageLabel) return;
    uint32_t totalLines = m_dram->getNumLines();
    uint32_t totalPages =
        (totalLines + DRAM_LINES_PER_PAGE - 1) / DRAM_LINES_PER_PAGE;
    uint32_t firstLine = m_dramPage * DRAM_LINES_PER_PAGE;
    uint32_t lastLine  = qMin(firstLine + DRAM_LINES_PER_PAGE - 1, totalLines - 1);

    m_dramPageLabel->setText(
        QString("Page %1 / %2   (lines %3 – %4)")
            .arg(m_dramPage + 1).arg(totalPages)
            .arg(firstLine).arg(lastLine));

    if (m_dramPrevBtn) m_dramPrevBtn->setEnabled(m_dramPage > 0);
    if (m_dramNextBtn) m_dramNextBtn->setEnabled(m_dramPage + 1 < totalPages);
}

void MainWindow::resetSimulationState() {
    if (m_cache)    m_cache->reset();
    if (m_dram)     m_dram->reset();
    if (m_regs)     m_regs->reset();
    if (m_pipeline) m_pipeline->reset();
    if (m_clock) {
        m_clock->reset();
        m_clock->clearBreakpoints();
    }
    if (m_bpList)   m_bpList->clear();   // sync visual list with clock state
    m_dramPage = 0;
    applySimulationMode();
}

void MainWindow::setRunControlsEnabled(bool enabled) {
    // Only used by onStep / executeStep path.
    // During a threaded run the enterRunningState/exitRunningState
    // pair manages control states instead, so skip if running.
    if (m_running) return;
    if (m_stepBtn) m_stepBtn->setEnabled(enabled);
    if (m_runBtn)  m_runBtn->setEnabled(enabled);
}