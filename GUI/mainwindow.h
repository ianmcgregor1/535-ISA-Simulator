#pragma once

#include <QMainWindow>
#include <QTableWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QTabWidget>
#include <QProgressBar>
#include <cstdint>
#include <vector>
#include <string>

// Forward declarations
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
    void onRunAll();
    void onReset();
    void onAssocChanged(int index);
    void onModeChanged(int index);
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
    void executeStep();
    void rebuildMemoryHierarchy(uint32_t assoc);
    void loadHexFile(const QString& path);
    void applySimulationMode();
    void resetSimulationState();
    void setRunControlsEnabled(bool enabled);

    // ── Widgets ──────────────────────────────────────────────────────────────
    QTabWidget*   m_tabs = nullptr;

    // Registers tab
    QTableWidget* m_intRegTable = nullptr;
    QTableWidget* m_floatRegTable = nullptr;

    // DRAM tab
    QTableWidget* m_dramTable = nullptr;

    // Cache tab
    QTableWidget* m_cacheTable = nullptr;

    // Pipeline tab
    QTableWidget* m_pipelineTable = nullptr;

    // Toolbar area
    QPushButton*  m_loadBtn = nullptr;
    QPushButton*  m_stepBtn = nullptr;
    QPushButton*  m_runBtn = nullptr;
    QPushButton*  m_resetBtn = nullptr;
    QComboBox*    m_assocCombo = nullptr;
    QComboBox*    m_modeCombo = nullptr;
    QComboBox*    m_stepModeCombo = nullptr;
    QSpinBox*     m_batchSpin = nullptr;
    QLabel*       m_fileLabel = nullptr;
    QLabel*       m_cycleLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;

    // Stats labels
    QLabel* m_statsLabel = nullptr;

    // ── Simulation state ─────────────────────────────────────────────────────
    static constexpr uint32_t DRAM_LINES  = 8192;
    static constexpr uint32_t DRAM_DELAY  = 3;
    static constexpr uint32_t CACHE_LINES = 8;
    static constexpr uint32_t CACHE_DELAY = 0;

    Memory*       m_dram  = nullptr;
    Memory*       m_cache = nullptr;
    RegisterFile* m_regs  = nullptr;
    Pipeline*     m_pipeline = nullptr;
    Clock*        m_clock = nullptr;

    int      m_batchSize   = 1;    // refresh every N instructions
    uint32_t m_assoc       = 4;
    QString  m_loadedFile;

    // Simulation mode
    enum class SimMode { ALL, NO_PIPELINE, NO_CACHE };
    SimMode  m_simMode = SimMode::ALL;

    enum class StepMode { CYCLE, INSTRUCTION };
    StepMode m_stepMode = StepMode::CYCLE;
};
