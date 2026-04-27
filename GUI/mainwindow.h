#pragma once

#include <QMainWindow>
#include <QTableWidget>
#include <QListWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QTabWidget>
#include <QProgressBar>
#include <QThread>
#include <atomic>
#include <cstdint>
#include <vector>
#include <string>

#include "../memory/memory.h"
#include "../core/registers.h"
#include "../core/pipeline.h"
#include "../core/clock.h"
#include "../core/decoder.h"
#include "../core/executor.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onLoadFile();
    void onStep();
    void onRunAll();       // toggles between RUN ALL and STOP
    void onReset();
    void onAssocChanged(int index);
    void onPipelineToggled(bool checked);
    void onCacheToggled(bool checked);
    void onDramPagePrev();
    void onDramPageNext();
    void onBatchSizeChanged(int val);
    void onRunFinished();  // called when the worker thread completes
    void onAddBP();        // adds m_bpSpin value to clock breakpoints
    void onClearBPs();     // calls clock->clearBreakpoints(), clears list
    void onRemoveBP(int row); // removes a single BP by row index

signals:
    void runFinished();    // emitted by worker thread back to GUI thread

private:
    // ── Layout builders ───────────────────────────────────────
    void buildToolbar();
    void buildRegisterAndPipelineTab();
    void buildMemoryTab();
    void buildStatsBar();

    // ── Refresh helpers ───────────────────────────────────────
    void refreshAll();
    void refreshRegisters();
    void refreshDram();
    void refreshCache();
    void refreshPipeline();
    void refreshStats();

    // ── Simulation helpers ────────────────────────────────────
    void executeStep();
    void rebuildMemoryHierarchy(uint32_t assoc);
    void loadHexFile(const QString& path);
    void applySimulationMode();
    void resetSimulationState();
    void setRunControlsEnabled(bool enabled);
    void updateDramPageLabel();
    void enterRunningState();   // switches button to STOP, disables other controls
    void exitRunningState();    // switches button back to RUN ALL, re-enables controls

    // ── Widgets ───────────────────────────────────────────────
    QTabWidget* m_tabs          = nullptr;

    // Combined Registers + Pipeline tab
    QTableWidget* m_intRegTable   = nullptr;
    QTableWidget* m_floatRegTable = nullptr;
    QTableWidget* m_pipelineTable = nullptr;

    // Combined Memory tab (DRAM paginated left, Cache right)
    QTableWidget* m_dramTable     = nullptr;
    QPushButton*  m_dramPrevBtn   = nullptr;
    QPushButton*  m_dramNextBtn   = nullptr;
    QLabel*       m_dramPageLabel = nullptr;
    QTableWidget* m_cacheTable    = nullptr;

    // Toolbar
    QPushButton*  m_loadBtn       = nullptr;
    QPushButton*  m_stepBtn       = nullptr;
    QPushButton*  m_runBtn        = nullptr;
    QPushButton*  m_resetBtn      = nullptr;
    QComboBox*    m_assocCombo    = nullptr;
    QCheckBox*    m_pipelineCheck = nullptr;
    QCheckBox*    m_cacheCheck    = nullptr;
    QComboBox*    m_stepModeCombo = nullptr;
    QSpinBox*     m_batchSpin     = nullptr;
    // Breakpoint panel (right of step mode combo)
    QListWidget*  m_bpList        = nullptr;   // running list of active BPs
    QSpinBox*     m_bpSpin        = nullptr;   // address input
    QPushButton*  m_addBpBtn      = nullptr;   // add BP to list + clock
    QPushButton*  m_clearBpBtn    = nullptr;   // clear all BPs
    QLabel*       m_fileLabel     = nullptr;
    QLabel*       m_cycleLabel    = nullptr;
    QLabel*       m_pcLabel       = nullptr;
    QProgressBar* m_progressBar   = nullptr;

    // Stats bar individual labels
    QLabel* m_statCacheHits    = nullptr;
    QLabel* m_statCacheMisses  = nullptr;
    QLabel* m_statMissRate     = nullptr;
    QLabel* m_statDRAMHits     = nullptr;
    QLabel* m_statDRAMMisses   = nullptr;
    QLabel* m_statCycles       = nullptr;
    QLabel* m_statRetired      = nullptr;

    // ── Simulation state ──────────────────────────────────────
    static constexpr uint32_t DRAM_LINES          = 8192;
    static constexpr uint32_t DRAM_DELAY          = 100;
    static constexpr uint32_t CACHE_LINES         = 32;
    static constexpr uint32_t CACHE_DELAY         = 1;
    static constexpr uint32_t DRAM_LINES_PER_PAGE = 32;

    Memory*       m_dram     = nullptr;
    Memory*       m_cache    = nullptr;
    RegisterFile* m_regs     = nullptr;
    Pipeline*     m_pipeline = nullptr;
    Clock*        m_clock    = nullptr;

    int      m_batchSize       = 1;    // refresh every N instructions or cycles
    int      m_assoc           = 4;
    uint32_t m_dramPage        = 0;
    QString  m_loadedFile;

    bool m_pipelineEnabled     = true;
    bool m_cacheEnabled        = true;

    // ── Run / Stop state ─────────────────────────────────────
    // m_running tracks whether Clock::run() is executing in the
    // worker thread. Stopping is done by calling Clock::pause()
    // which sets the clock's internal halted flag — the run()
    // loop checks it at the top of every iteration and exits.
    std::atomic<bool> m_stopRequested{false};  // guards destructor race only
    bool              m_running      = false;
    QThread*          m_workerThread = nullptr;

    enum class StepMode { CYCLE, INSTRUCTION };
    StepMode m_stepMode = StepMode::CYCLE;
};