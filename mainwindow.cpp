#include <QTableWidget>
#include <QHeaderView>
#include <QMenuBar>
#include "mainwindow.h"
#include <QApplication>
#include <QMessageBox>
#include <QDateTime>
#include <QFont>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QRegularExpression>

// ─── Helpers ────────────────────────────────────────────────────────────────

static QIcon makeColorIcon(const QColor &color) {
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    p.drawEllipse(1, 1, 14, 14);
    return QIcon(pm);
}

// ─── Constructor ────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("SCX Scheduler Manager");
    setMinimumSize(640, 520);
    resize(720, 580);

    setupUi();
    setupMenuBar();
    setupTray();
    setupConnections();

    autoRefreshTimer = new QTimer(this);
    autoRefreshTimer->setInterval(5000);
    connect(autoRefreshTimer, &QTimer::timeout, this, &MainWindow::refreshStatus);
    autoRefreshTimer->start();

    // Initial data load
    refreshSchedulerList();
    refreshStatus();
}

MainWindow::~MainWindow() {}

// ─── UI Setup ────────────────────────────────────────────────────────────────

void MainWindow::setupUi() {
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto *rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setContentsMargins(12, 12, 12, 8);
    rootLayout->setSpacing(8);

    // ── Header bar ──────────────────────────────────────────────────────────
    auto *headerFrame = new QFrame;
    headerFrame->setObjectName("headerFrame");
    auto *headerLayout = new QHBoxLayout(headerFrame);
    headerLayout->setContentsMargins(12, 8, 12, 8);

    auto *titleLabel = new QLabel("⚡ SCX Scheduler Manager");
    titleLabel->setObjectName("titleLabel");
    QFont tf = titleLabel->font();
    tf.setPointSize(14);
    tf.setBold(true);
    titleLabel->setFont(tf);

    statusDot = new QLabel("●");
    statusDot->setObjectName("statusDotStopped");
    statusDot->setToolTip("Scheduler status");
    QFont df = statusDot->font();
    df.setPointSize(16);
    statusDot->setFont(df);

    statusLabel = new QLabel("Stopped");
    statusLabel->setObjectName("statusLabelStopped");

    refreshBtn = new QPushButton("↻ Refresh");
    refreshBtn->setMinimumWidth(120);
    refreshBtn->setFixedHeight(34);
    refreshBtn->setObjectName("secondaryBtn");

    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(statusDot);
    headerLayout->addWidget(statusLabel);
    headerLayout->addSpacing(8);
    headerLayout->addWidget(refreshBtn);

    rootLayout->addWidget(headerFrame);

    // ── Tabs ────────────────────────────────────────────────────────────────
    tabWidget = new QTabWidget;
    tabWidget->setObjectName("mainTabs");

    // ── Tab 1: Status ────────────────────────────────────────────────────────
    auto *statusTab = new QWidget;
    auto *statusLayout = new QVBoxLayout(statusTab);
    statusLayout->setContentsMargins(12, 12, 12, 12);
    statusLayout->setSpacing(10);

    auto *currentGroup = new QGroupBox("Current Scheduler");
    auto *currentGrid = new QGridLayout(currentGroup);
    currentGrid->setSpacing(8);

    auto addInfoRow = [&](QGridLayout *g, int row, const QString &label, QLabel *&valLabel) {
        auto *lbl = new QLabel(label);
        lbl->setObjectName("infoKey");
        valLabel = new QLabel("—");
        valLabel->setObjectName("infoVal");
        g->addWidget(lbl, row, 0);
        g->addWidget(valLabel, row, 1);
    };

    addInfoRow(currentGrid, 0, "Scheduler:", schedulerNameLabel);
    addInfoRow(currentGrid, 1, "Mode:", modeLabel);
    addInfoRow(currentGrid, 2, "Service:", serviceStatusLabel);
    currentGrid->setColumnStretch(1, 1);

    stopBtn = new QPushButton("⏹  Stop Scheduler");
    stopBtn->setObjectName("dangerBtn");
    stopBtn->setEnabled(false);
    stopBtn->setFixedHeight(36);

    statusLayout->addWidget(currentGroup);
    statusLayout->addWidget(stopBtn);
    statusLayout->addStretch();

    tabWidget->addTab(statusTab, "Status");

    // ── Tab 2: Control ───────────────────────────────────────────────────────
    auto *controlTab = new QWidget;
    auto *controlLayout = new QVBoxLayout(controlTab);
    controlLayout->setContentsMargins(12, 12, 12, 12);
    controlLayout->setSpacing(10);

    auto *schedGroup = new QGroupBox("Scheduler Selection");
    auto *schedGrid = new QGridLayout(schedGroup);
    schedGrid->setSpacing(8);

    auto *schedLabel = new QLabel("Scheduler:");
    schedLabel->setObjectName("infoKey");
    schedulerCombo = new QComboBox;
    schedulerCombo->setPlaceholderText("Loading…");
    schedulerCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *modeKeyLabel = new QLabel("Mode/Profile:");
    modeKeyLabel->setObjectName("infoKey");
    modeCombo = new QComboBox;
    modeCombo->addItems({"Auto", "Gaming", "Lowlatency", "Powersave"});

    auto *flagsKeyLabel = new QLabel("Custom flags:");
    flagsKeyLabel->setObjectName("infoKey");
    customFlagsEdit = new QLineEdit;
    customFlagsEdit->setPlaceholderText("e.g. --slice-us 5000 (optional)");

    schedGrid->addWidget(schedLabel,      0, 0);
    schedGrid->addWidget(schedulerCombo,  0, 1);
    schedGrid->addWidget(modeKeyLabel,    1, 0);
    schedGrid->addWidget(modeCombo,       1, 1);
    schedGrid->addWidget(flagsKeyLabel,   2, 0);
    schedGrid->addWidget(customFlagsEdit, 2, 1);
    schedGrid->setColumnStretch(1, 1);

    auto *btnRow = new QHBoxLayout;
    startBtn  = new QPushButton("▶  Start");
    switchBtn = new QPushButton("⇄  Switch");
    listBtn   = new QPushButton("⊞  Refresh List");
    startBtn->setObjectName("primaryBtn");
    switchBtn->setObjectName("primaryBtn");
    listBtn->setObjectName("secondaryBtn");
    startBtn->setFixedHeight(36);
    switchBtn->setFixedHeight(36);
    listBtn->setFixedHeight(36);
    btnRow->addWidget(startBtn);
    btnRow->addWidget(switchBtn);
    btnRow->addWidget(listBtn);

    autostartCheck = new QCheckBox("Enable scx_loader service on boot (systemctl enable)");

    controlLayout->addWidget(schedGroup);
    controlLayout->addLayout(btnRow);
    controlLayout->addWidget(autostartCheck);
    controlLayout->addStretch();

    tabWidget->addTab(controlTab, "Control");

    // ── Tab 3: Log ───────────────────────────────────────────────────────────
    auto *logTab = new QWidget;
    auto *logLayout = new QVBoxLayout(logTab);
    logLayout->setContentsMargins(8, 8, 8, 8);
    logLayout->setSpacing(6);

    logView = new QTextEdit;
    logView->setReadOnly(true);
    logView->setObjectName("logView");
    QFont monof;
    monof.setFamily("Monospace");
    monof.setPointSize(9);
    logView->setFont(monof);

    clearLogBtn = new QPushButton("Clear Log");
    clearLogBtn->setObjectName("secondaryBtn");
    clearLogBtn->setFixedWidth(100);

    auto *logBtnRow = new QHBoxLayout;
    logBtnRow->addStretch();
    logBtnRow->addWidget(clearLogBtn);

    logLayout->addWidget(logView);
    logLayout->addLayout(logBtnRow);

    tabWidget->addTab(logTab, "Log");

    // ── Tab 4: Scheduler Reference ───────────────────────────────────────────
    auto *refTab = new QWidget;
    auto *refLayout = new QVBoxLayout(refTab);
    refLayout->setContentsMargins(10, 10, 10, 10);
    refLayout->setSpacing(8);

    auto *refTitle = new QLabel("Scheduler Reference");
    refTitle->setObjectName("refTitle");
    QFont rtf = refTitle->font();
    rtf.setPointSize(11);
    rtf.setBold(true);
    refTitle->setFont(rtf);

    auto *refSubtitle = new QLabel("Use this table to choose the right scheduler for your workload.");
    refSubtitle->setObjectName("infoKey");

    // Build the table
    auto *table = new QTableWidget(refTab);
    table->setObjectName("refTable");
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({"Scheduler", "Best For", "Avoid When", "Notes"});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->setWordWrap(true);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    struct SchedInfo {
        QString name;
        QString bestFor;
        QString avoidWhen;
        QString notes;
    };

    const QList<SchedInfo> scheds = {
        {
            "scx_bpfland",
            "General desktop, interactive apps, mixed workloads",
            "Pure batch/CPU-bound workloads where throughput matters more than latency",
            "Recommended default. Prioritises tasks that block frequently (interactive). "
            "Cache-topology aware — keeps tasks near their L2/L3 cache. Runs entirely in BPF (low overhead)."
        },
        {
            "scx_lavd",
            "Gaming, audio production, real-time / low-latency workloads",
            "Battery-constrained devices (can increase power draw); pure server workloads",
            "Latency-Aware Virtual Deadline scheduler. Computes a latency-criticality score per task. "
            "Core Compaction keeps active cores at higher frequency when load < 50%, saving power. "
            "Autopilot mode auto-switches between performance/balanced/powersave."
        },
        {
            "scx_rusty",
            "Multi-core desktops and servers, NUMA systems, general-purpose",
            "Single-core or very low core-count machines",
            "Partitions CPUs by last-level cache domain to minimise cross-cache migration. "
            "Good scalability on high core-count systems. Hybrid BPF + userspace design."
        },
        {
            "scx_flash",
            "High-throughput batch jobs, compilation, video encoding, servers",
            "Latency-sensitive interactive use; gaming",
            "Optimised for throughput over responsiveness. "
            "Minimal overhead; suited to sustained CPU-bound workloads."
        },
        {
            "scx_p2dq",
            "Mixed desktop/server, systems needing fair load balancing",
            "Workloads requiring strict real-time guarantees",
            "Pick-2 randomised load balancing keeps queues shallow. "
            "Simple design means low scheduler overhead. PELT-based load tracking."
        },
        {
            "scx_nest",
            "Systems where boosting clock speed matters; lightly-loaded desktops",
            "Heavily loaded systems (all cores busy)",
            "Places tasks on already-warm, high-frequency cores to keep turbo boost active. "
            "Effective when CPU utilisation is low to moderate."
        },
        {
            "scx_simple",
            "Testing, debugging, learning sched_ext, minimal systems",
            "Any production workload",
            "Minimal FIFO/least-runtime policy. No topology awareness. "
            "Useful as a baseline for benchmarking other schedulers."
        },
        {
            "scx_layered",
            "Power users who want per-application scheduling policies",
            "Users who don't want to write a config file",
            "Classifies threads into named layers (like cgroups) and applies a different "
            "policy to each. Highly flexible but requires manual JSON configuration."
        },
        {
            "scx_cosmos",
            "General desktop and server, mixed workloads, NUMA systems",
            "Workloads requiring strict real-time guarantees",
            "Lightweight locality-first scheduler. Keeps tasks on the same CPU using local DSQs when the system is not saturated — reduces cache misses and locking contention. "
            "Under saturation, switches to a deadline-based policy with a shared DSQ so interactive tasks can preempt CPU-bound ones. "
            "Uses 10µs time slices by default. Good general-purpose choice with low overhead."
        },
        {
            "scx_beerland",
            "General desktop, similar workloads to scx_bpfland but lower overhead",
            "Workloads needing the full feature set of bpfland",
            "A reduced-overhead variant of scx_bpfland by the same author. "
            "Strips back some of bpfland's more expensive per-task tracking to lower scheduler overhead on busy systems. "
            "Good alternative if bpfland feels heavy on your hardware."
        },
        {
            "scx_tickless",
            "Cloud computing, virtualisation, HPC, server batch workloads",
            "Latency-sensitive or interactive desktop use — nohz_full adds syscall overhead",
            "Routes all scheduling events through a small pool of primary CPUs, allowing other CPUs to run tickless (no scheduler interrupts). "
            "Reduces OS noise for VM guests and HPC jobs. Requires booting with nohz_full= kernel parameter to fully realise the benefit. "
            "Not designed for desktop or gaming use."
        },
        {
            "scx_pandemonium",
            "Experimental use, testing dynamic task-behaviour learning",
            "Production workloads — this is experimental",
            "Written in Rust and C. Dynamically learns task behaviour over time to improve scheduling decisions. "
            "CachyOS experimental scheduler; not yet in upstream scx. May change significantly between updates."
        },
        {
            "scx_cake",
            "Experimental desktop/gaming use on CachyOS",
            "Production workloads — this is experimental",
            "CachyOS-specific experimental scheduler. Not yet in upstream scx. "
            "Intended for desktop and gaming workloads. Behaviour and flags may change significantly between updates — check the CachyOS Discord for current status."
        },
        {
            "scx_rustland",
            "Experimentation, userspace scheduler research",
            "Production use — higher latency than BPF-only schedulers",
            "Predecessor to scx_bpfland. Scheduling decisions run in userspace (Rust), "
            "which adds a context-switch overhead on every scheduling event. "
            "Interesting for learning how userspace schedulers work but not recommended for daily use."
        },
    };

    table->setRowCount(scheds.size());
    for (int i = 0; i < scheds.size(); ++i) {
        const auto &s = scheds[i];
        auto *nameItem = new QTableWidgetItem(s.name);
        nameItem->setFont(QFont("Monospace", 9));
        nameItem->setForeground(QColor("#ff9900"));
        nameItem->setData(Qt::UserRole, s.name); // for filtering later if needed
        table->setItem(i, 0, nameItem);

        for (const auto &[col, text] : {
                std::pair{1, s.bestFor},
                std::pair{2, s.avoidWhen},
                std::pair{3, s.notes}}) {
            auto *item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
            table->setItem(i, col, item);
        }
    }
    // Must be called after columns are sized and items set so Qt can measure correctly
    QTimer::singleShot(0, table, [table]() { table->resizeRowsToContents(); });

    refLayout->addWidget(refTitle);
    refLayout->addWidget(refSubtitle);
    refLayout->addWidget(table);
    refTableWidget = table;  // store for resizeEvent

    tabWidget->addTab(refTab, "Reference");

    // ── Tab 5: Custom Flags Reference ────────────────────────────────────────
    auto *flagsTab = new QWidget;
    auto *flagsTabLayout = new QVBoxLayout(flagsTab);
    flagsTabLayout->setContentsMargins(10, 10, 10, 10);
    flagsTabLayout->setSpacing(8);

    auto *flagsTitle = new QLabel("Custom Flags Reference");
    flagsTitle->setObjectName("refTitle");
    QFont ftf = flagsTitle->font();
    ftf.setPointSize(11);
    ftf.setBold(true);
    flagsTitle->setFont(ftf);

    auto *flagsSubtitle = new QLabel("Pass these via the Custom flags field in the Control tab. Select a scheduler to filter.");
    flagsSubtitle->setObjectName("infoKey");

    // Scheduler filter combo
    auto *filterRow = new QHBoxLayout;
    auto *filterLabel = new QLabel("Scheduler:");
    filterLabel->setObjectName("infoKey");
    auto *flagsFilterCombo = new QComboBox;
    flagsFilterCombo->addItems({"All",
        "scx_bpfland", "scx_beerland", "scx_cake", "scx_cosmos",
        "scx_flash", "scx_lavd", "scx_layered", "scx_nest",
        "scx_p2dq", "scx_pandemonium", "scx_rusty", "scx_rustland",
        "scx_simple", "scx_tickless"});
    filterRow->addWidget(filterLabel);
    filterRow->addWidget(flagsFilterCombo);
    filterRow->addStretch();

    auto *flagsTable = new QTableWidget;
    flagsTable->setObjectName("refTable");
    flagsTable->setColumnCount(4);
    flagsTable->setHorizontalHeaderLabels({"Scheduler", "Flag", "Values", "Description"});
    flagsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    flagsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    flagsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    flagsTable->setColumnWidth(2, 140);
    flagsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    flagsTable->verticalHeader()->setVisible(false);
    flagsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    flagsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    flagsTable->setAlternatingRowColors(true);
    flagsTable->setShowGrid(false);
    flagsTable->setWordWrap(true);
    flagsTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    struct FlagInfo {
        QString sched;   // bare name, e.g. "scx_bpfland"
        QString flag;
        QString values;
        QString description;
    };

    const QList<FlagInfo> flags = {
        // scx_bpfland
        {"scx_bpfland", "-s / --slice-us",      "<microseconds>",  "Time slice duration in microseconds. Lower = more responsive. Default: 5000. Try 500–2000 for low-latency gaming."},
        {"scx_bpfland", "-m / --sched-mode",    "performance\npowersave",   "CPU frequency governor hint. 'performance' keeps clocks high; 'powersave' allows down-clocking."},
        {"scx_bpfland", "-w / --keep-running",  "(flag)",          "Prevents idle CPUs from being offlined. Useful on desktops to reduce wakeup latency."},
        {"scx_bpfland", "-k / --local-kthreads","(flag)",          "Pin kernel threads to their originating CPU's local DSQ. Can improve I/O throughput."},
        {"scx_bpfland", "-I / --idle-resume-us","<microseconds>",  "How long a CPU must be idle before it is considered for resume. Tune for power vs. latency trade-off."},
        {"scx_bpfland", "-t / --idle-cpus-pct", "<percent>",       "Target percentage of idle CPUs to maintain. Higher = more power saving; lower = faster task dispatch."},
        {"scx_bpfland", "-S / --stop-once",     "(flag)",          "Stop the scheduler cleanly on the first error rather than restarting. Useful for debugging."},

        // scx_lavd
        {"scx_lavd",    "--performance",         "(flag)",          "Enable performance mode: disables power management and keeps all cores at high frequency. Best for gaming."},
        {"scx_lavd",    "--powersave",            "(flag)",          "Enable powersave mode: aggressively packs tasks onto fewer cores, allows others to sleep."},
        {"scx_lavd",    "--no-core-compaction",  "(flag)",          "Disable core compaction. Tasks spread freely across all cores rather than being consolidated."},
        {"scx_lavd",    "--autopilot",            "(flag)",          "Let LAVD auto-switch between performance/balanced/powersave based on load. Overrides manual mode flags."},
        {"scx_lavd",    "--slice-boost-max-x",   "<multiplier>",    "Maximum time slice boost multiplier for latency-critical tasks. Default: 10. Higher = more burst time for interactive tasks."},

        // scx_rusty
        {"scx_rusty",   "--slice-us",            "<microseconds>",  "Base time slice in microseconds. Default: 2000. Reduce for interactive work; increase for batch."},
        {"scx_rusty",   "--interval",            "<milliseconds>",  "Scheduling decision interval. Lower = faster reaction to load changes but more overhead. Default: 100."},
        {"scx_rusty",   "--tune-util",           "<percent>",       "Target CPU utilisation percentage per LLC domain. Helps balance load across cache domains on NUMA systems."},
        {"scx_rusty",   "--greedy-idle-cpu",     "(flag)",          "Allow tasks to immediately steal idle CPUs outside their preferred LLC domain."},

        // scx_flash
        {"scx_flash",   "-m / --sched-mode",    "performance\npowersave\nall",  "'performance' boosts interactive tasks; 'powersave' conserves energy; 'all' treats all tasks equally."},
        {"scx_flash",   "-w / --keep-running",  "(flag)",          "Prevent CPUs from idling. Reduces wakeup latency at the cost of power."},
        {"scx_flash",   "-C / --no-preempt",    "0 / 1",           "Disable preemption of running tasks by newly woken ones. 0 = allow preemption (default); 1 = disable."},
        {"scx_flash",   "-s / --slice-us",      "<microseconds>",  "Base time slice. Default: 5000."},
        {"scx_flash",   "-S / --slice-max-us",  "<microseconds>",  "Maximum allowed time slice. Default: 20000."},
        {"scx_flash",   "-I / --idle-cpu-thresh","<percent>",      "Idle CPU threshold below which Flash stops trying to pack tasks."},
        {"scx_flash",   "-D / --dispatch-order","(flag)",          "Honour task priority during dispatch rather than pure FIFO."},
        {"scx_flash",   "-L / --llc-affinity",  "(flag)",          "Prefer keeping tasks within their LLC domain to maximise cache reuse."},

        // scx_p2dq
        {"scx_p2dq",    "--task-slice true",     "(bool)",          "Enable per-task dynamic time slices instead of a fixed global slice. Improves fairness under mixed workloads."},
        {"scx_p2dq",    "-f / --fifo-fallback",  "(flag)",          "Fall back to FIFO ordering within queues when load is low."},
        {"scx_p2dq",    "--sched-mode",          "performance",     "Enable performance scheduling mode: reduced idle time, higher clock frequency hints."},
        {"scx_p2dq",    "-y / --yield-preempt",  "(flag)",          "Allow tasks that call sched_yield() to preempt the current task immediately."},
        {"scx_p2dq",    "--select-idle-in-enqueue", "(flag)",       "Select an idle CPU at enqueue time rather than at dispatch. Can reduce latency on lightly loaded systems."},

        // scx_cosmos
        {"scx_cosmos",  "-s / --slice-us",       "<microseconds>",  "Time slice duration. Default: 10µs — very short, designed to minimise latency. Increase (e.g. 5000) for throughput-heavy workloads."},
        {"scx_cosmos",  "--numa",                "(flag)",          "Enable per-NUMA-node shared DSQs under saturation, instead of a single global DSQ. Improves locality on multi-socket systems."},
        {"scx_cosmos",  "--stats",               "<interval_s>",    "Print scheduler statistics every N seconds. Useful for observing behaviour without scxtop."},

        // scx_tickless
        {"scx_tickless", "--primary-domain",     "<cpumask>",       "Hex CPU mask for the primary scheduling CPUs. Default: only CPU 0. On large systems, e.g. --primary-domain 0xf assigns CPUs 0–3. More primaries = better task distribution but more overhead."},
        {"scx_tickless", "--frequency",          "<hz>",            "How often (in Hz) primary CPUs check tickless CPUs for contention. Lower = less overhead, higher = faster response to imbalance. Default: 1000."},
        {"scx_tickless", "--stats",              "<interval_s>",    "Print scheduling statistics every N seconds."},

        // scx_layered
        {"scx_layered",  "--config / -f",        "<path.json>",     "Path to the JSON layer configuration file. Required — scx_layered does nothing useful without a config. See /usr/share/scx/examples/ for templates."},
        {"scx_layered",  "--stats",              "<interval_s>",    "Print per-layer statistics every N seconds."},
        {"scx_layered",  "--slice-us",           "<microseconds>",  "Base time slice for all layers. Individual layers can override this in their config."},
        {"scx_layered",  "--exit-dump-len",      "<lines>",         "Number of lines of BPF ringbuf output to dump on exit. Useful for debugging layer misclassification."},

        // scx_nest
        {"scx_nest",     "--primary-cpus",       "<count>",         "Number of 'primary' CPUs to keep warm at high frequency. Tasks preferentially run here when load is low."},
        {"scx_nest",     "--slice-us",           "<microseconds>",  "Base time slice. Default: 20000."},
        {"scx_nest",     "--r-imbalance",        "<threshold>",     "Load imbalance ratio that triggers task migration away from the primary nest. Lower = more aggressive rebalancing."},

        // Schedulers with no user-facing flags
        {"scx_rusty",    "(none beyond basic)",  "—",               "scx_rusty exposes --slice-us, --interval, --tune-util, and --greedy-idle-cpu as noted above. All other behaviour is automatic."},
        {"scx_rustland",  "(no flags)",          "—",               "scx_rustland has no user-facing configuration flags. All scheduling decisions are made automatically by the userspace Rust runtime."},
        {"scx_simple",    "(no flags)",          "—",               "scx_simple is intentionally minimal with no configuration flags. Use it as-is for testing or benchmarking."},
        {"scx_beerland",  "(no flags)",          "—",               "scx_beerland has no documented user-facing flags. It operates with the same defaults as scx_bpfland but with reduced overhead."},
        {"scx_pandemonium","(no flags)",         "—",               "scx_pandemonium is a CachyOS experimental scheduler. No stable flags documented — run scx_pandemonium --help on your install to check current options."},
        {"scx_cake",      "(no flags)",          "—",               "scx_cake is a CachyOS experimental scheduler. No stable flags documented — run scx_cake --help on your install to check current options."},
    };

    // Populate table and hook up filter
    auto populateFlags = [flagsTable, flags](const QString &filter) {
        flagsTable->setRowCount(0);
        for (const auto &f : flags) {
            if (filter != "All" && f.sched != filter) continue;
            const int row = flagsTable->rowCount();
            flagsTable->insertRow(row);

            auto *schedItem = new QTableWidgetItem(f.sched);
            schedItem->setFont(QFont("Monospace", 9));
            schedItem->setForeground(QColor("#ff9900"));
            schedItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
            flagsTable->setItem(row, 0, schedItem);

            auto *flagItem = new QTableWidgetItem(f.flag);
            flagItem->setFont(QFont("Monospace", 9));
            flagItem->setForeground(QColor("#00ff00"));
            flagItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
            flagsTable->setItem(row, 1, flagItem);

            auto *valItem = new QTableWidgetItem(f.values);
            valItem->setFont(QFont("Monospace", 9));
            valItem->setForeground(QColor("#ff9900"));
            valItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
            flagsTable->setItem(row, 2, valItem);

            auto *descItem = new QTableWidgetItem(f.description);
            descItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
            flagsTable->setItem(row, 3, descItem);
        }
        QTimer::singleShot(0, flagsTable, [flagsTable]() { flagsTable->resizeRowsToContents(); });
    };

    populateFlags("All");

    // Connect filter — capture by value so populateFlags lambda is safe
    QObject::connect(flagsFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     flagsTab, [flagsFilterCombo, populateFlags](int) {
        populateFlags(flagsFilterCombo->currentText());
    });

    flagsTabLayout->addWidget(flagsTitle);
    flagsTabLayout->addWidget(flagsSubtitle);
    flagsTabLayout->addLayout(filterRow);
    flagsTabLayout->addWidget(flagsTable);
    flagsTableWidget = flagsTable;  // store for resizeEvent

    tabWidget->addTab(flagsTab, "Flags");

    // Resize rows whenever the user switches to either reference tab
    connect(tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        Q_UNUSED(index);
        if (refTableWidget)   refTableWidget->resizeRowsToContents();
        if (flagsTableWidget) flagsTableWidget->resizeRowsToContents();
    });

    rootLayout->addWidget(tabWidget);

    // ── Style — brand style guide ─────────────────────────────────────────────
    // Colours: bg #0b0f0b · panel #0f160f · border rgba(0,255,0,0.18)
    //          accent #00ff00 · cmd/code #ff9900 · danger #ff4444 · text #f9f9f9
    // Font: Roboto Mono (monospace stack fallback)
    setStyleSheet(R"(
        QMainWindow, QWidget {
            background-color: #0b0f0b;
            color: #f9f9f9;
            font-family: "Roboto Mono", ui-monospace, "SFMono-Regular", Menlo, Monaco,
                         Consolas, "Liberation Mono", "Courier New", monospace;
            font-size: 9.5pt;
        }

        /* ── Header ── */
        #headerFrame {
            background: linear-gradient(180deg, rgba(0,255,0,0.06), rgba(0,0,0,0)), #0f160f;
            border: 1px solid rgba(0,255,0,0.18);
            border-radius: 14px;
        }
        #titleLabel {
            color: #00ff00;
            font-weight: 700;
        }
        #statusDotStopped  { color: #ff4444; }
        #statusDotRunning  { color: #00ff00; }
        #statusLabelStopped { color: #ff4444; }
        #statusLabelRunning { color: #00ff00; }

        /* ── GroupBox (panel style) ── */
        QGroupBox {
            background: linear-gradient(180deg, rgba(0,255,0,0.06), rgba(0,0,0,0)), #0f160f;
            border: 1px solid rgba(0,255,0,0.18);
            border-radius: 14px;
            margin-top: 12px;
            padding: 8px;
            color: #00ff00;
            font-weight: 700;
            font-size: 9pt;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 4px;
            color: #00ff00;
        }

        /* ── Info labels ── */
        #infoKey { color: rgba(249,249,249,0.75); }
        #infoVal { color: #f9f9f9; font-weight: 700; }

        /* ── Buttons ── */
        QPushButton {
            border-radius: 10px;
            padding: 6px 14px;
            font-weight: 700;
            font-family: "Roboto Mono", monospace;
        }
        #primaryBtn {
            background: rgba(0,0,0,0.55);
            color: #00ff00;
            border: 1px solid rgba(0,255,0,0.75);
        }
        #primaryBtn:hover  { background: #00ff00; color: #000000; border-color: #00ff00; }
        #primaryBtn:pressed { background: rgba(0,255,0,0.70); color: #000000; }
        #primaryBtn:disabled { background: rgba(0,0,0,0.30); color: rgba(249,249,249,0.25); border-color: rgba(0,255,0,0.12); }

        #secondaryBtn {
            background: rgba(0,0,0,0.55);
            color: rgba(249,249,249,0.75);
            border: 1px solid rgba(0,255,0,0.18);
        }
        #secondaryBtn:hover { background: rgba(0,255,0,0.10); color: #f9f9f9; border-color: rgba(0,255,0,0.40); }

        #dangerBtn {
            background: rgba(255,68,68,0.08);
            color: #ff4444;
            border: 1px solid #ff4444;
        }
        #dangerBtn:hover   { background: #ff4444; color: #000000; }
        #dangerBtn:pressed { background: rgba(255,68,68,0.70); }
        #dangerBtn:disabled {
            background: rgba(0,0,0,0.20);
            color: rgba(249,249,249,0.25);
            border: 1px solid rgba(249,249,249,0.12);
        }

        /* ── Inputs ── */
        QComboBox, QLineEdit {
            background: rgba(0,0,0,0.60);
            border: 1px solid rgba(255,153,0,0.28);
            border-radius: 10px;
            padding: 5px 8px;
            color: #ff9900;
            selection-background-color: rgba(255,153,0,0.22);
        }
        QComboBox:focus, QLineEdit:focus {
            border-color: rgba(0,255,0,0.75);
            color: #f9f9f9;
        }
        QComboBox QAbstractItemView {
            background: #0f160f;
            border: 1px solid rgba(0,255,0,0.18);
            color: #f9f9f9;
            selection-background-color: rgba(0,255,0,0.15);
        }
        QComboBox::drop-down { border: none; }
        QComboBox::down-arrow { image: none; width: 0; }

        /* ── Tabs ── */
        QTabWidget::pane {
            border: 1px solid rgba(0,255,0,0.18);
            border-radius: 14px;
            background: #0f160f;
        }
        QTabBar::tab {
            background: #0b0f0b;
            color: rgba(249,249,249,0.75);
            padding: 7px 20px;
            border: 1px solid rgba(0,255,0,0.18);
            border-bottom: none;
            border-top-left-radius: 10px;
            border-top-right-radius: 10px;
            margin-right: 2px;
            font-weight: 700;
        }
        QTabBar::tab:selected {
            background: #0f160f;
            color: #00ff00;
            border-bottom-color: #0f160f;
        }
        QTabBar::tab:hover { background: rgba(0,255,0,0.06); color: #f9f9f9; }

        /* ── Log view ── */
        #logView {
            background: rgba(0,0,0,0.60);
            border: 1px solid rgba(255,153,0,0.28);
            border-radius: 12px;
            color: #f9f9f9;
        }

        /* ── Checkbox ── */
        QCheckBox { color: rgba(249,249,249,0.75); spacing: 6px; }
        QCheckBox::indicator {
            width: 16px; height: 16px;
            border-radius: 4px;
            border: 1px solid rgba(0,255,0,0.40);
            background: rgba(0,0,0,0.55);
        }
        QCheckBox::indicator:checked {
            background: #00ff00;
            border-color: #00ff00;
        }

        /* ── Scrollbar ── */
        QScrollBar:vertical {
            background: #0b0f0b;
            width: 8px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background: rgba(0,255,0,0.28);
            border-radius: 4px;
            min-height: 20px;
        }
        QScrollBar::handle:vertical:hover { background: rgba(0,255,0,0.55); }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }

        /* ── Status bar ── */
        QStatusBar {
            background: #0f160f;
            color: rgba(249,249,249,0.75);
            font-size: 8.5pt;
            border-top: 1px solid rgba(0,255,0,0.18);
        }

        /* ── Reference tab ── */
        #refTitle { color: #00ff00; font-weight: 700; }
        #refSubtitle { color: rgba(249,249,249,0.75); }

        #refTable {
            background: rgba(0,0,0,0.60);
            border: 1px solid rgba(0,255,0,0.18);
            border-radius: 12px;
            gridline-color: rgba(0,255,0,0.10);
            color: #f9f9f9;
            font-size: 9pt;
        }
        #refTable::item {
            padding: 8px 10px;
            border: none;
        }
        #refTable::item:selected {
            background: rgba(0,255,0,0.12);
            color: #00ff00;
        }
        #refTable::item:alternate {
            background: rgba(0,255,0,0.04);
        }
        QHeaderView::section {
            background: #0f160f;
            color: #00ff00;
            font-weight: 700;
            font-size: 8.5pt;
            padding: 6px 8px;
            border: none;
            border-bottom: 1px solid rgba(0,255,0,0.28);
        }
    )");

    statusBar()->showMessage("Ready");
}

void MainWindow::setupMenuBar() {
    auto *helpMenu = menuBar()->addMenu("Help");
    auto *aboutAction = helpMenu->addAction("About SCX Scheduler Manager");
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);

    // Style the menu bar to match the brand
    menuBar()->setStyleSheet(R"(
        QMenuBar {
            background: #0b0f0b;
            color: rgba(249,249,249,0.75);
            border-bottom: 1px solid rgba(0,255,0,0.18);
            font-family: "Roboto Mono", monospace;
            font-size: 9pt;
        }
        QMenuBar::item:selected { background: rgba(0,255,0,0.10); color: #f9f9f9; }
        QMenu {
            background: #0f160f;
            border: 1px solid rgba(0,255,0,0.18);
            color: #f9f9f9;
            font-family: "Roboto Mono", monospace;
        }
        QMenu::item:selected { background: rgba(0,255,0,0.15); color: #00ff00; }
    )");
}

void MainWindow::setupTray() {
    trayIcon = new QSystemTrayIcon(makeColorIcon(Qt::gray), this);
    trayMenu = new QMenu(this);

    trayStatusAction = trayMenu->addAction("Status: Unknown");
    trayStatusAction->setEnabled(false);
    trayMenu->addSeparator();
    trayStartAction = trayMenu->addAction("Start Scheduler");
    trayStopAction  = trayMenu->addAction("Stop Scheduler");
    trayMenu->addSeparator();
    trayMenu->addAction("Show Window", this, [this]{ show(); raise(); activateWindow(); });
    trayQuitAction = trayMenu->addAction("Quit", qApp, &QApplication::quit);

    trayIcon->setContextMenu(trayMenu);
    trayIcon->setToolTip("SCX Scheduler Manager");
    trayIcon->show();

    connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);
    connect(trayStartAction, &QAction::triggered, this, &MainWindow::startScheduler);
    connect(trayStopAction,  &QAction::triggered, this, &MainWindow::stopScheduler);
}

void MainWindow::setupConnections() {
    connect(refreshBtn,      &QPushButton::clicked, this, &MainWindow::refreshStatus);
    connect(startBtn,        &QPushButton::clicked, this, &MainWindow::startScheduler);
    connect(stopBtn,         &QPushButton::clicked, this, &MainWindow::stopScheduler);
    connect(switchBtn,       &QPushButton::clicked, this, &MainWindow::switchScheduler);
    connect(listBtn,         &QPushButton::clicked, this, &MainWindow::refreshSchedulerList);
    connect(clearLogBtn,     &QPushButton::clicked, this, &MainWindow::clearLog);
    connect(autostartCheck,  &QCheckBox::toggled,   this, &MainWindow::toggleServiceAutostart);
}

// ─── scxctl runner ───────────────────────────────────────────────────────────

void MainWindow::runScxctl(const QStringList &args, const QString &description) {
    QString desc = description.isEmpty() ? ("scxctl " + args.join(" ")) : description;
    appendLog(QString("[%1] Running: scxctl %2")
                  .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                  .arg(args.join(" ")), "#00ff00");

    pendingOperation = args.value(0);
    auto *proc = new QProcess(this);
    currentOutput.clear();

    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc](){
        currentOutput += proc->readAllStandardOutput();
    });
    connect(proc, &QProcess::readyReadStandardError, this, [this, proc](){
        QString err = proc->readAllStandardError();
        appendLog(err.trimmed(), "#ff4444");
    });
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onProcessFinished);
    connect(proc, &QProcess::errorOccurred, this, &MainWindow::onProcessError);

    proc->start("pkexec", QStringList{"scxctl"} + args);
    statusBar()->showMessage(desc + "…");
}

void MainWindow::onProcessFinished(int exitCode, QProcess::ExitStatus) {
    auto *proc = qobject_cast<QProcess*>(sender());
    if (proc) proc->deleteLater();

    if (!currentOutput.trimmed().isEmpty())
        appendLog(currentOutput.trimmed(), "#00ff00");

    if (exitCode == 0) {
        statusBar()->showMessage("Done ✓", 3000);
    } else if (exitCode == 126 || exitCode == 127) {
        // pkexec exit 126 = authorisation cancelled by user, 127 = not found
        const QString msg = (exitCode == 126)
            ? "Authorisation cancelled — operation aborted."
            : "pkexec not found — is polkit installed?";
        appendLog(msg, "#ff9900");
        statusBar()->showMessage(msg, 5000);
    } else {
        appendLog(QString("Process exited with code %1").arg(exitCode), "#ff4444");
        statusBar()->showMessage(QString("Error (exit %1)").arg(exitCode), 4000);
    }

    // After any operation, refresh status
    if (pendingOperation != "get")
        refreshStatus();
}

void MainWindow::onProcessError(QProcess::ProcessError err) {
    auto *proc = qobject_cast<QProcess*>(sender());
    if (proc) proc->deleteLater();

    QString msg;
    switch (err) {
        case QProcess::FailedToStart: msg = "Failed to start scxctl (is it installed? is polkit running?)"; break;
        case QProcess::Crashed:       msg = "scxctl crashed"; break;
        default:                      msg = "Process error"; break;
    }
    appendLog(msg, "#ff4444");
    statusBar()->showMessage(msg, 5000);
}

// ─── Actions ─────────────────────────────────────────────────────────────────

void MainWindow::refreshStatus() {
    auto *proc = new QProcess(this);

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();

        // "no scx scheduler running" is a valid stopped state, not an error
        const bool explicitlyStopped = out.contains("no scx scheduler", Qt::CaseInsensitive)
                                    || out.contains("not running",       Qt::CaseInsensitive);

        if (exitCode != 0 || out.isEmpty() || explicitlyStopped) {
            // Only update UI if state actually changed — suppresses log spam
            if (isRunning || currentScheduler != QString()) {
                isRunning = false;
                currentScheduler.clear();
                currentMode.clear();
                updateStatusIndicator(false);
            }
            return;
        }

        // Output when running: "running bpfland in auto mode"
        QRegularExpression re(R"(running (\S+) in (\S+) mode)", QRegularExpression::CaseInsensitiveOption);
        const auto m = re.match(out);
        if (m.hasMatch()) {
            const QString sched = m.captured(1);
            const QString mode  = m.captured(2);
            // Only update UI if something actually changed
            if (!isRunning || currentScheduler != sched || currentMode != mode) {
                currentScheduler = sched;
                currentMode      = mode;
                isRunning        = true;
                updateStatusIndicator(true, currentScheduler);
            }
        } else {
            // Truly unexpected — log once per unique message, then treat as stopped
            if (isRunning || currentScheduler != QString()) {
                appendLog("scxctl get: " + out, "#ff9900");
                isRunning = false;
                currentScheduler.clear();
                currentMode.clear();
                updateStatusIndicator(false);
            }
        }
    });

    connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError) {
        proc->deleteLater();
        if (isRunning) {
            isRunning = false;
            currentScheduler.clear();
            currentMode.clear();
            updateStatusIndicator(false);
        }
    });

    proc->start("scxctl", {"get"});
}

void MainWindow::refreshSchedulerList() {
    schedulerCombo->clear();
    supportedSchedulers.clear();
    schedulerCombo->setPlaceholderText("Loading…");
    statusBar()->showMessage("Fetching scheduler list…");

    // Output format: supported schedulers: ["bpfland", "flash", "lavd", "p2dq", "rusty"]
    // scxctl list needs no root — run directly.

    auto *proc = new QProcess(this);

    // Accumulate stdout into the QProcess object itself via readAll() in finished handler.
    // Do NOT capture local variables by reference in async lambdas.
    connect(proc, &QProcess::readyReadStandardError, this, [this, proc]() {
        QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        if (!err.isEmpty())
            appendLog("scxctl list: " + err, "rgba(249,249,249,0.75)");
    });

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus) {
        // Drain all stdout here — safe because finished fires after all output is ready
        const QString output = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();

        appendLog("scxctl list output: " + output, "rgba(249,249,249,0.75)");

        if (exitCode == 0 && output.contains('"')) {
            QRegularExpression re(R"x("([^"]+)")x");
            QRegularExpressionMatchIterator it = re.globalMatch(output);
            while (it.hasNext()) {
                const QString s = it.next().captured(1).trimmed();
                if (!s.isEmpty() && !supportedSchedulers.contains(s)) {
                    supportedSchedulers << s;
                    schedulerCombo->addItem("scx_" + s, s);
                }
            }
        }

        if (schedulerCombo->count() == 0) {
            appendLog("scx_loader not reachable or no schedulers found — using built-in list", "#ff9900");
            const QStringList fallback{
                "bpfland","beerland","cake","cosmos","flash",
                "lavd","layered","nest","p2dq","pandemonium",
                "rusty","rustland","simple","tickless"
            };
            for (const QString &s : fallback) {
                schedulerCombo->addItem("scx_" + s, s);
                supportedSchedulers << s;
            }
            appendLog("Tip: ensure scx_loader.service is running for the real list", "rgba(249,249,249,0.75)");
        }

        appendLog(QString("Scheduler list ready: %1 schedulers").arg(schedulerCombo->count()), "#00ff00");
        statusBar()->showMessage(QString("%1 schedulers available").arg(schedulerCombo->count()), 3000);
    });

    connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError error) {
        proc->deleteLater();
        if (error == QProcess::FailedToStart)
            appendLog("'scxctl' not found — is scx-tools installed?", "#ff4444");
        if (schedulerCombo->count() == 0) {
            const QStringList fallback{
                "bpfland","beerland","cake","cosmos","flash",
                "lavd","layered","nest","p2dq","pandemonium",
                "rusty","rustland","simple","tickless"
            };
            for (const QString &s : fallback) {
                schedulerCombo->addItem("scx_" + s, s);
                supportedSchedulers << s;
            }
        }
        statusBar()->showMessage("Using built-in scheduler list", 3000);
    });

    proc->start("scxctl", {"list"});
}

void MainWindow::startScheduler() {
    // userData holds the bare name (e.g. "bpfland"); scxctl wants that, not "scx_bpfland"
    QString sched = schedulerCombo->currentData().toString();
    if (sched.isEmpty()) sched = schedulerCombo->currentText().remove("scx_");
    if (sched.isEmpty()) { statusBar()->showMessage("Select a scheduler first", 3000); return; }

    QStringList args{"start", "-s", sched};
    QString mode = modeCombo->currentText().toLower();
    if (mode != "auto") args << "-m" << mode;
    QString flags = customFlagsEdit->text().trimmed();
    if (!flags.isEmpty()) args << "-a" << flags; // scxctl uses -a / --args

    runScxctl(args, QString("Starting scx_%1").arg(sched));
}

void MainWindow::stopScheduler() {
    runScxctl({"stop"}, "Stopping scheduler");
}

void MainWindow::switchScheduler() {
    QString sched = schedulerCombo->currentData().toString();
    if (sched.isEmpty()) sched = schedulerCombo->currentText().remove("scx_");
    if (sched.isEmpty()) { statusBar()->showMessage("Select a scheduler first", 3000); return; }

    QStringList args{"switch", "-s", sched};
    QString mode = modeCombo->currentText().toLower();
    if (mode != "auto") args << "-m" << mode;

    runScxctl(args, QString("Switching to scx_%1").arg(sched));
}

void MainWindow::toggleServiceAutostart() {
    bool enable = autostartCheck->isChecked();
    QStringList args = enable
        ? QStringList{"systemctl", "enable", "--now", "scx_loader.service"}
        : QStringList{"systemctl", "disable", "--now", "scx_loader.service"};

    auto *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, enable](int exitCode, QProcess::ExitStatus) {
        proc->deleteLater();
        if (exitCode == 126) {
            appendLog("Authorisation cancelled — service state unchanged.", "#ff9900");
            // Revert the checkbox since nothing actually changed
            QSignalBlocker blocker(autostartCheck);
            autostartCheck->setChecked(!enable);
        } else {
            appendLog(QString("scx_loader service %1 %2")
                          .arg(enable ? "enabled" : "disabled")
                          .arg(exitCode == 0 ? "✓" : "✗ (exit " + QString::number(exitCode) + ")"),
                      exitCode == 0 ? "#00ff00" : "#ff4444");
        }
        refreshStatus();
    });
    proc->start("pkexec", args);
}

void MainWindow::clearLog() { logView->clear(); }

void MainWindow::showAbout() {
    QMessageBox about(this);
    about.setWindowTitle("About SCX Scheduler Manager");
    about.setTextFormat(Qt::RichText);
    about.setText(
        "<b style='color:#00ff00;font-family:monospace'>SCX Scheduler Manager</b> v1.0.0<br><br>"
        "A Qt6 GUI for managing sched-ext BPF schedulers<br>"
        "via <code style='color:#ff9900'>scxctl</code> and <code style='color:#ff9900'>scx_loader</code>.<br><br>"
        "Designed for Fedora with the CachyOS kernel.<br><br>"
        "<a style='color:#00ff00' href='https://github.com/sched-ext/scx'>sched-ext upstream</a> &nbsp;|&nbsp; "
        "<a style='color:#00ff00' href='https://wiki.cachyos.org'>CachyOS wiki</a>"
    );
    about.setStyleSheet(
        "QMessageBox { background:#0b0f0b; color:#f9f9f9; font-family:'Roboto Mono',monospace; }"
        "QLabel { color:#f9f9f9; }"
        "QPushButton { background:rgba(0,0,0,0.55); color:#00ff00; border:1px solid rgba(0,255,0,0.75);"
        "  border-radius:8px; padding:5px 14px; font-family:'Roboto Mono',monospace; }"
        "QPushButton:hover { background:#00ff00; color:#000000; }"
    );
    about.exec();
}

// ─── Status Update ───────────────────────────────────────────────────────────

void MainWindow::updateStatusIndicator(bool running, const QString &schedulerName) {
    if (running) {
        statusDot->setObjectName("statusDotRunning");
        statusLabel->setObjectName("statusLabelRunning");
        statusLabel->setText("Running");
        schedulerNameLabel->setText("scx_" + schedulerName);
        modeLabel->setText(currentMode.isEmpty() ? "—" : currentMode);
        serviceStatusLabel->setText("active");
        serviceStatusLabel->setStyleSheet("color:#00ff00; font-weight:bold;");
        stopBtn->setEnabled(true);
        trayStatusAction->setText("Status: scx_" + schedulerName);
        trayIcon->setIcon(makeColorIcon("#00ff00"));
        trayIcon->setToolTip("SCX: running scx_" + schedulerName);
    } else {
        statusDot->setObjectName("statusDotStopped");
        statusLabel->setObjectName("statusLabelStopped");
        statusLabel->setText("Stopped");
        schedulerNameLabel->setText("—");
        modeLabel->setText("—");
        serviceStatusLabel->setText("inactive");
        serviceStatusLabel->setStyleSheet("color:rgba(249,249,249,0.40); font-weight:bold;");
        stopBtn->setEnabled(false);
        trayStatusAction->setText("Status: Stopped");
        trayIcon->setIcon(makeColorIcon("#ff4444"));
        trayIcon->setToolTip("SCX: No scheduler running");
    }
    // Force Qt to re-evaluate the objectName-based stylesheet rules
    statusDot->style()->polish(statusDot);
    statusLabel->style()->polish(statusLabel);
}

// ─── Log ─────────────────────────────────────────────────────────────────────

void MainWindow::appendLog(const QString &text, const QString &color) {
    const QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    logView->append(QString("<span style='color:%1'>[%2] %3</span>")
                        .arg(color, ts, text.toHtmlEscaped()));
}

// ─── Tray ─────────────────────────────────────────────────────────────────────

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::DoubleClick) {
        setVisible(!isVisible());
        if (isVisible()) { raise(); activateWindow(); }
    }
}
