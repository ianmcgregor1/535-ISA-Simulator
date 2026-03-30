#pragma once

#include <QMainWindow>
#include <QTableWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QTabWidget>
#include <QStatusBar>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QProgressBar>
#include <cstdint>
#include <vector>
#include <string>

// Forward declarations — replace with your actual headers
#include "../memory/memory.h"
#include "../core/registers.h"
#include "../core/pipeline.h"
#include "../core/clock.h"
#include "../core/decoder.h"
#include "../core/executor.h"

// Worker thread for running many instructions without blocking the UI
class SimWorker : public QThread {
    Q_OBJECT
public:
    SimWorker(Memory* cache, Memory* dram, RegisterFile* regs,
              int instructionCount, QObject* parent = nullptr);
    void run() override;

signals:
    void progress(int completed);
    void finished();

private:
    Memory*       cache;
    Memory*       dram;
    RegisterFile* regs;
    int           count;
};

// ─────────────────────────────────────────────────────────────────────────────

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onLoadFile();
    void onStepInstruction();
    void onRunAll();
    void onReset();
    void onAssocChanged(int index);
    void onModeChanged(int index);
    void onWorkerProgress(int completed);
    void onWorkerDone();
    void onBatchSizeChanged(int val);

private:
    // ── Layout builders ──────────────────────────────────────────────────────
    void buildToolbar();
    void buildRegisterTable();
    void buildDramTable();
    void buildCacheTable();
    void buildPipelineTable();
    void buildStatsBar();

    // ── Refresh helpers (batch-aware) ────────────────────────────────────────
    void refreshAll();
    void refreshRegisters();
    void refreshDram();
    void refreshCache();
    void refreshPipeline();
    void refreshStats();

    // ── Simulation helpers ───────────────────────────────────────────────────
    void executeSingleInstruction();
    void rebuildMemoryHierarchy(uint32_t assoc);
    void loadHexFile(const QString& path);

    // ── Widgets ──────────────────────────────────────────────────────────────
    QTabWidget*   m_tabs;

    // Registers tab
    QTableWidget* m_intRegTable;
    QTableWidget* m_floatRegTable;

    // DRAM tab
    QTableWidget* m_dramTable;

    // Cache tab
    QTableWidget* m_cacheTable;

    // Pipeline tab
    QTableWidget* m_pipelineTable;

    // Toolbar area
    QPushButton*  m_loadBtn;
    QPushButton*  m_stepBtn;
    QPushButton*  m_runBtn;
    QPushButton*  m_resetBtn;
    QComboBox*    m_assocCombo;
    QComboBox*    m_modeCombo;
    QSpinBox*     m_batchSpin;   // update every N instructions
    QLabel*       m_fileLabel;
    QLabel*       m_cycleLabel;
    QProgressBar* m_progressBar;

    // Stats labels
    QLabel* m_statsLabel;

    // ── Simulation state ─────────────────────────────────────────────────────
    static constexpr uint32_t DRAM_LINES  = 8192;
    static constexpr uint32_t DRAM_DELAY  = 3;
    static constexpr uint32_t CACHE_LINES = 8;
    static constexpr uint32_t CACHE_DELAY = 0;

    Memory*       m_dram  = nullptr;
    Memory*       m_cache = nullptr;
    RegisterFile* m_regs  = nullptr;

    int      m_cmdCount    = 0;
    int      m_batchSize   = 1;    // refresh every N instructions
    int      m_sinceRefresh= 0;    // instructions executed since last refresh
    uint32_t m_assoc       = 4;
    QString  m_loadedFile;

    SimWorker* m_worker = nullptr;

    // Simulation mode
    enum class SimMode { ALL, NO_PIPELINE, NO_CACHE };
    SimMode  m_simMode = SimMode::ALL;
};
