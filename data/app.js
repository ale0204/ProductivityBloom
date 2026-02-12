/**
 * Productivity Bloom - Web Interface
 * Comunicare cu ESP32 prin API REST și WebSocket
 */

// ============================================
// Configuration & State
// ============================================

// Use current page hostname (works for both AP mode and WiFi mode)
const ESP32_IP = window.location.hostname || '192.168.4.1';

const CONFIG = {
    API_BASE: `http://${ESP32_IP}`,
    WS_URL: `ws://${ESP32_IP}:81`,
    RECONNECT_INTERVAL: 2000,
    UPDATE_INTERVAL: 1000
};

// Debug log
console.log('Productivity Bloom loading, ESP32 IP:', ESP32_IP);

// Application State
const state = {
    connected: false,
    wsConnected: false,  // Track WebSocket specifically
    status: 'idle', // idle, focusing, break, withered
    currentTask: null,
    timeLeft: 0,
    totalTime: 0,
    plant: {
        stage: 0,
        isWithered: false
    },
    tasks: [],
    stats: {
        completed: 0,
        total: 0,
        focusMinutes: 0
    },
    ws: null,
    pendingWater: 0,  // Câte "udări" sunt disponibile (task-uri completate neapoi udate)
    wateredCount: 0,  // Câte task-uri au fost "confirmate" prin udare (pentru afișare)
    dailyGoal: 0,     // Obiectivul zilnic setat de utilizator
    currentSessionGoal: 0, // Câte task-uri noi trebuie făcute în sesiunea curentă
    goalLocked: false, // Dacă obiectivul a fost confirmat
    selectedTaskId: 0,  // Task pregătit pentru pornire cu flip MPU
    showingConfirmModal: false,  // Modal de confirmare flip
    flipCancelledWaitingFlipBack: false  // Waiting for user to flip back after cancel
};

// Plant stages configuration
const PLANT_STAGES = [
    { emoji: '🌰', name: 'Samanta' },
    { emoji: '🌱', name: 'Lastar' },
    { emoji: '🌿', name: 'Crestere' },
    { emoji: '🌸', name: 'Inflorit' }
];

// ============================================
// DOM Elements
// ============================================

const elements = {
    // Connection
    connectionStatus: document.getElementById('connectionStatus'),
    
    // Status
    statusLabel: document.getElementById('statusLabel'),
    currentTask: document.getElementById('currentTask'),
    timerValue: document.getElementById('timerValue'),
    timerBar: document.getElementById('timerBar'),
    
    // Plant
    plantStage: document.getElementById('plantStage'),
    plantStageText: document.getElementById('plantStageText'),
    stageNumber: document.getElementById('stageNumber'),
    plantHealth: document.getElementById('plantHealth'),
    plantVisual: document.getElementById('plantVisual'),
    waterHint: document.getElementById('waterHint'),
    btnWater: document.getElementById('btnWater'),
    btnKill: document.getElementById('btnKill'),
    
    // Task Form
    taskName: document.getElementById('taskName'),
    focusTime: document.getElementById('focusTime'),
    breakTime: document.getElementById('breakTime'),
    btnAddTask: document.getElementById('btnAddTask'),
    
    // Daily Goal
    dailyGoalInput: document.getElementById('dailyGoalInput'),
    btnSetGoal: document.getElementById('btnSetGoal'),
    btnResetGoal: document.getElementById('btnResetGoal'),
    goalDisplay: document.getElementById('goalDisplay'),
    goalSection: document.getElementById('goalSection'),
    
    // Task List
    taskItems: document.getElementById('taskItems'),
    taskCount: document.getElementById('taskCount'),
    emptyState: document.getElementById('emptyState'),
    
    // Stats
    tasksCompleted: document.getElementById('tasksCompleted'),
    tasksTotal: document.getElementById('tasksTotal'),
    focusTotal: document.getElementById('focusTotal'),
    progressPercent: document.getElementById('progressPercent'),
    progressFill: document.getElementById('progressFill'),
    currentTime: document.getElementById('currentTime'),
    btnSyncTime: document.getElementById('btnSyncTime'),
    btnResetDay: document.getElementById('btnResetDay'),
    
    // Weekly Stats
    weeklyTasks: document.getElementById('weeklyTasks'),
    weeklyFocus: document.getElementById('weeklyFocus'),
    weeklyAvg: document.getElementById('weeklyAvg'),
    weeklyChart: document.getElementById('weeklyChart'),
    bestDay: document.getElementById('bestDay'),
    
    // Footer
    espIP: document.getElementById('espIP'),
    
    // Toast
    toast: document.getElementById('toast'),
    toastIcon: document.getElementById('toastIcon'),
    toastMessage: document.getElementById('toastMessage'),
    
    // Modal
    modalOverlay: document.getElementById('modalOverlay'),
    modalIcon: document.getElementById('modalIcon'),
    modalTitle: document.getElementById('modalTitle'),
    modalMessage: document.getElementById('modalMessage'),
    modalCancel: document.getElementById('modalCancel'),
    modalConfirm: document.getElementById('modalConfirm')
};

// ============================================
// WebSocket Connection
// ============================================

// Helper to check if we're in development mode (only localhost)
function isDevMode() {
    const hostname = window.location.hostname;
    // Only treat as dev mode if literally on localhost
    return hostname === '127.0.0.1' || hostname === 'localhost';
}

function connectWebSocket() {
    console.log('connectWebSocket called, isDevMode:', isDevMode());
    
    // Pentru dezvoltare fără ESP32, folosim mock
    if (isDevMode()) {
        console.log('Development mode - using mock data');
        setConnectionStatus(true);
        loadMockData();
        return;
    }
    
    // Close existing connection if any
    if (state.ws) {
        try {
            state.ws.close();
        } catch (e) {}
        state.ws = null;
    }
    
    try {
        console.log('Attempting WebSocket connection to:', CONFIG.WS_URL);
        state.ws = new WebSocket(CONFIG.WS_URL);
        
        state.ws.onopen = () => {
            console.log('WebSocket connected successfully');
            state.wsConnected = true;
            setConnectionStatus(true);
            showToast('Conectat la ESP32!', 'success');
            // Refresh stats on reconnect
            fetchWeeklyStats();
        };
        
        state.ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                handleWebSocketMessage(data);
            } catch (e) {
                console.error('Failed to parse WS message:', e);
            }
        };
        
        state.ws.onclose = () => {
            console.log('WebSocket disconnected');
            state.wsConnected = false;
            setConnectionStatus(false);
            // Reconnect after delay
            setTimeout(connectWebSocket, CONFIG.RECONNECT_INTERVAL);
        };
        
        state.ws.onerror = (error) => {
            console.error('WebSocket error:', error, 'URL:', CONFIG.WS_URL);
            setConnectionStatus(false);
        };
        
    } catch (e) {
        console.error('WebSocket connection failed:', e);
        setConnectionStatus(false);
        setTimeout(connectWebSocket, CONFIG.RECONNECT_INTERVAL);
    }
}

function handleWebSocketMessage(data) {
    switch (data.type) {
        case 'status':
            updateStatus(data);
            break;
        case 'plant':
            updatePlant(data);
            break;
        case 'tasks':
            state.tasks = data.tasks || [];
            renderTasks();
            break;
        case 'stats':
            updateStats(data);
            break;
        case 'flip':
            showToast('Cub întors! ' + (data.paused ? 'Pauză' : 'Continui'), 'success');
            break;
        case 'revive':
            showReviveAnimation();
            showToast(data.message || 'Planta a reinviat!', 'success');
            break;
    }
}

function sendWebSocketMessage(data) {
    if (state.ws && state.ws.readyState === WebSocket.OPEN) {
        state.ws.send(JSON.stringify(data));
    } else {
        // Fallback to REST API
        sendAPIRequest(data);
    }
}

// ============================================
// API Communication
// ============================================

async function sendAPIRequest(action) {
    // Pentru dezvoltare, simulăm răspunsuri
    if (isDevMode()) {
        return handleMockAction(action);
    }
    
    try {
        const response = await fetch(`${CONFIG.API_BASE}/api/action`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(action)
        });
        return await response.json();
    } catch (e) {
        console.error('API request failed:', e);
        showToast('Eroare de conexiune', 'error');
        return null;
    }
}

async function fetchTasks() {
    if (isDevMode()) {
        return; // Mock data already loaded
    }
    
    try {
        const response = await fetch(`${CONFIG.API_BASE}/api/tasks`);
        const data = await response.json();
        state.tasks = data.tasks || [];
        renderTasks();
    } catch (e) {
        console.error('Failed to fetch tasks:', e);
    }
}

async function fetchStatus() {
    console.log('fetchStatus called, isDevMode:', isDevMode());
    
    if (isDevMode()) {
        return;
    }
    
    const url = `${CONFIG.API_BASE}/api/status`;
    console.log('Fetching status from:', url);
    
    try {
        const response = await fetch(url);
        console.log('fetchStatus response:', response.status);
        const data = await response.json();
        console.log('fetchStatus data:', data);
        updateStatus(data);
        updatePlant(data.plant);
        updateStats(data.stats);
        // If we got here, we have connectivity (even if WebSocket failed)
        if (!state.connected) {
            setConnectionStatus(true);
            console.log('Connected via API polling (WebSocket unavailable)');
        }
    } catch (e) {
        console.error('Failed to fetch status:', e);
        setConnectionStatus(false);
    }
}

// ============================================
// UI Update Functions
// ============================================

function setConnectionStatus(connected) {
    state.connected = connected;
    const statusDot = elements.connectionStatus.querySelector('.status-dot');
    const statusText = elements.connectionStatus.querySelector('.status-text');
    
    statusDot.className = 'status-dot ' + (connected ? 'connected' : 'disconnected');
    statusText.textContent = connected ? 'Conectat' : 'Deconectat';
}

function updateStatus(data) {
    const prevStatus = state.status;
    const prevTask = state.currentTask;
    
    state.status = data.state || 'idle';
    state.timeLeft = data.timeLeft || 0;
    state.totalTime = data.totalTime || 0;
    state.currentTask = data.taskName || null;
    
    // Check if ESP32 is waiting for flip confirmation
    // But NOT if we're already showing the "flip back" message
    if (data.waitingForConfirmation && !state.showingConfirmModal && !state.flipCancelledWaitingFlipBack) {
        showFlipConfirmModal();
    }
    
    // If no longer waiting and we were waiting for flip back, hide modal and resume
    if (!data.waitingForConfirmation && state.flipCancelledWaitingFlipBack) {
        hideModal();
        state.flipCancelledWaitingFlipBack = false;
        showToast('Timer resumed!', 'success');
    }
    
    // Update status display
    const statusConfig = {
        idle: { icon: '||', label: 'IDLE', class: '' },
        focusing: { icon: '>>', label: 'FOCUSING', class: 'focusing' },
        paused: { icon: '||', label: 'PAUSED', class: 'paused' },
        break: { icon: '..', label: 'ON BREAK', class: 'break' },
        withered: { icon: 'X', label: 'PLANT WITHERED', class: 'withered' }
    };
    
    const config = statusConfig[state.status] || statusConfig.idle;
    
    elements.statusLabel.textContent = config.label;
    elements.statusLabel.className = 'status-label ' + config.class;
    elements.currentTask.textContent = state.currentTask || 'Niciun task activ';
    
    // Update timer
    updateTimerDisplay();
    
    // Enable/disable water button
    elements.btnWater.disabled = state.status !== 'idle' || state.plant.isWithered;
    
    // Only re-render tasks if status or current task changed (avoid flickering)
    if (prevStatus !== state.status || prevTask !== state.currentTask) {
        renderTasks();
        
        // Refresh stats when transitioning to idle or break (session ended)
        if (state.status === 'idle' || state.status === 'break') {
            fetchWeeklyStats();
        }
    }
}

function updateTimerDisplay() {
    const minutes = Math.floor(state.timeLeft / 60);
    const seconds = state.timeLeft % 60;
    elements.timerValue.textContent = 
        `${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;
    
    // Update progress bar
    if (state.totalTime > 0) {
        const progress = ((state.totalTime - state.timeLeft) / state.totalTime) * 100;
        elements.timerBar.style.width = `${progress}%`;
    } else {
        elements.timerBar.style.width = '0%';
    }
}

function updatePlant(data) {
    if (data) {
        state.plant.stage = data.stage !== undefined ? data.stage : state.plant.stage;
        state.plant.isWithered = data.isWithered || false;
        
        // Update these from ESP32 data if available
        if (data.pendingWater !== undefined) state.pendingWater = data.pendingWater;
        if (data.wateredCount !== undefined) state.wateredCount = data.wateredCount;
        if (data.dailyGoal !== undefined) {
            state.dailyGoal = data.dailyGoal;
            state.goalLocked = data.dailyGoal > 0;
        }
        if (data.totalGoal !== undefined) state.currentSessionGoal = data.totalGoal;
    }
    
    // Calculează stadiul bazat pe task-uri completate vs obiectiv
    const completedTasks = state.tasks.filter(t => t.completed).length;
    const targetTasks = state.dailyGoal > 0 ? state.dailyGoal : state.tasks.length;
    
    // Folosește stage-ul din state (care e actualizat la water)
    const displayStage = state.plant.isWithered ? 0 : state.plant.stage;
    const stageInfo = PLANT_STAGES[displayStage] || PLANT_STAGES[0];
    
    // Update plant visual
    if (state.plant.isWithered) {
        elements.plantStage.textContent = '🥀';
        elements.plantStage.classList.add('withered');
        elements.plantStageText.textContent = 'Ofilita';
        elements.plantHealth.innerHTML = '<span class="health-icon"></span><span class="health-text" style="color: var(--danger);">Ofilita</span>';
    } else {
        // Afișează emoji-ul stadiului
        elements.plantStage.textContent = stageInfo.emoji;
        elements.plantStage.classList.remove('withered');
        elements.plantStageText.textContent = stageInfo.name;
        elements.plantHealth.innerHTML = '<span class="health-icon"></span><span class="health-text">Sanatoasa</span>';
    }
    
    // Afișează progresul: (task-uri udate/confirmate / task-uri noi necesare)
    const displayGoal = state.currentSessionGoal > 0 ? state.currentSessionGoal : state.tasks.length;
    if (state.tasks.length === 0) {
        elements.stageNumber.textContent = `(0/0)`;
    } else {
        elements.stageNumber.textContent = `(${state.wateredCount}/${displayGoal})`;
    }
    
    // Water button: activ doar dacă există task-uri completate care nu au fost încă udate
    // și planta nu e ofilită și nu suntem în focus mode
    const canWater = state.pendingWater > 0 && 
                     !state.plant.isWithered && 
                     state.status !== 'focusing' &&
                     state.plant.stage < 3;
    elements.btnWater.disabled = !canWater;
    
    // Actualizează hint-ul
    if (elements.waterHint) {
        const totalTasks = state.tasks.length;
        const goalSet = state.dailyGoal > 0;
        
        if (state.plant.stage >= 3 || (totalTasks > 0 && state.tasks.filter(t => t.completed).length >= totalTasks && state.pendingWater === 0)) {
            elements.waterHint.innerHTML = '<small>Planta a inflorit complet! Felicitari!</small>';
            elements.waterHint.className = 'water-hint complete';
        } else if (state.pendingWater > 0) {
            elements.waterHint.innerHTML = `<small>Poti uda planta! (${state.pendingWater} udari disponibile)</small>`;
            elements.waterHint.className = 'water-hint ready';
        } else if (totalTasks === 0) {
            elements.waterHint.innerHTML = '<small>Adauga task-uri pentru a incepe</small>';
            elements.waterHint.className = 'water-hint';
        } else if (!goalSet) {
            elements.waterHint.innerHTML = '<small>Seteaza obiectivul zilnic pentru a incepe</small>';
            elements.waterHint.className = 'water-hint';
        } else {
            elements.waterHint.innerHTML = '<small>Completeaza un task pentru a putea uda planta</small>';
            elements.waterHint.className = 'water-hint';
        }
    }
    
    // Actualizează afișarea obiectivului
    updateGoalDisplay();
}

// Funcție pentru afișarea și gestionarea obiectivului zilnic
function updateGoalDisplay() {
    if (!elements.goalDisplay) return;
    
    const completedTasks = state.tasks.filter(t => t.completed).length;
    
    if (state.goalLocked && state.dailyGoal > 0) {
        elements.goalDisplay.innerHTML = `
            <div class="goal-info">
                <span class="goal-target">Obiectiv: ${completedTasks}/${state.dailyGoal} task-uri</span>
                <button class="btn btn-sm btn-secondary" id="btnChangeGoal">Modifica</button>
            </div>
        `;
        elements.goalSection.classList.add('locked');
        
        // Adaugă event listener pentru butonul de modificare
        const btnChange = document.getElementById('btnChangeGoal');
        if (btnChange) {
            btnChange.onclick = resetGoal;
        }
    } else {
        elements.goalDisplay.innerHTML = '';
        elements.goalSection.classList.remove('locked');
    }
}

// Setează obiectivul zilnic
function setDailyGoal() {
    const goalValue = parseInt(elements.dailyGoalInput.value) || 0;
    
    console.log('setDailyGoal called:', { goalValue, currentDailyGoal: state.dailyGoal, goalLocked: state.goalLocked });
    
    if (goalValue < 1) {
        showToast('Introdu un obiectiv valid (minim 1 task)', 'error');
        return;
    }
    
    if (goalValue > 20) {
        showToast('Obiectivul maxim este 20 de task-uri', 'error');
        return;
    }
    
    // Dacă există un goal anterior (și e locked sau nu), noul goal trebuie să fie strict mai mare
    if (state.dailyGoal > 0 && goalValue <= state.dailyGoal) {
        showToast(`Noul obiectiv trebuie sa fie mai mare decat ${state.dailyGoal}!`, 'error');
        console.log('Validation failed: goalValue <= state.dailyGoal');
        return;
    }
    
    // Calculează câte task-uri NOI trebuie făcute
    const previousGoal = state.dailyGoal;
    const newTasksNeeded = previousGoal > 0 ? (goalValue - previousGoal) : goalValue;
    
    state.dailyGoal = goalValue;
    state.currentSessionGoal = newTasksNeeded; // Task-uri noi de făcut
    state.goalLocked = true;
    
    // Resetează planta complet pentru noul obiectiv
    state.plant.stage = 0;
    state.pendingWater = 0;
    state.wateredCount = 0;  // Reset complet
    
    // NU recalcula pendingWater - utilizatorul trebuie să completeze task-uri noi
    
    updatePlant();
    showToast(`Obiectiv setat: ${goalValue} task-uri! Planta a fost resetata.`, 'success');
    
    sendWebSocketMessage({
        action: 'setGoal',
        goal: goalValue
    });
}

// Resetează obiectivul
function resetGoal() {
    showModal(
        '?',
        'Modifica Obiectivul',
        `Vrei sa modifici obiectivul zilnic? Noul obiectiv trebuie sa fie mai mare decat ${state.dailyGoal}. Planta va fi resetata.`,
        () => {
            // Salvează goal-ul vechi pentru validare
            const oldGoal = state.dailyGoal;
            
            state.goalLocked = false;
            // Nu reseta dailyGoal la 0, păstrează-l pentru validare
            
            if (elements.dailyGoalInput) {
                // Setează valoarea minimă la goal-ul curent + 1
                elements.dailyGoalInput.value = oldGoal + 1;
                elements.dailyGoalInput.min = oldGoal + 1;
            }
            
            updatePlant();
            showToast(`Seteaza un obiectiv mai mare decat ${oldGoal}!`, 'success');
        }
    );
}

function updateStats(data) {
    if (!data) return;
    
    state.stats.completed = data.completed || 0;
    state.stats.total = data.total || 0;
    state.stats.focusMinutes = data.focusMinutes || 0;
    
    elements.tasksCompleted.textContent = state.stats.completed;
    elements.tasksTotal.textContent = state.stats.total;
    elements.focusTotal.textContent = state.stats.focusMinutes;
    
    const percent = state.stats.total > 0 
        ? Math.round((state.stats.completed / state.stats.total) * 100) 
        : 0;
    
    elements.progressPercent.textContent = `${percent}%`;
    elements.progressFill.style.width = `${percent}%`;
}

// ============================================
// Task Management
// ============================================

function renderTasks() {
    const container = elements.taskItems;
    
    if (state.tasks.length === 0) {
        container.innerHTML = `
            <div class="empty-state">
                <span class="empty-icon">--</span>
                <span class="empty-text">Niciun task adaugat</span>
            </div>
        `;
        elements.taskCount.textContent = '0 task-uri';
        return;
    }
    
    container.innerHTML = state.tasks.map(task => {
        const isSelected = state.selectedTaskId === task.id;
        const isActive = task.active || (state.status === 'focusing' && state.currentTask === task.name);
        
        // Determine hint text based on state
        let hintText = '';
        if (isSelected && !isActive) {
            hintText = '<div class="task-hint">Flip the cube to start!</div>';
        } else if (isActive && state.status === 'focusing') {
            hintText = '<div class="task-hint active">Flip back when done</div>';
        }
        // No hint for completed tasks or idle state
        
        return `
        <div class="task-item ${task.completed ? 'completed' : ''} ${isActive ? 'active' : ''} ${isSelected ? 'selected' : ''}" data-id="${task.id}">
            <div class="task-checkbox ${task.completed ? 'checked' : ''} ${!task.started ? 'disabled' : ''}" 
                 ${task.started ? `onclick="toggleTaskComplete(${task.id})"` : `onclick="showToast('Trebuie sa pornesti task-ul inainte!', 'error')"`}>
            </div>
            <div class="task-info">
                <div class="task-name">${escapeHtml(task.name)}</div>
                <div class="task-duration">Focus: ${task.focusDuration}min / Pauza: ${task.breakDuration}min</div>
                ${hintText}
            </div>
            <div class="task-actions">
                ${!task.completed && !isActive ? `
                    ${!isSelected ? `<button class="task-btn select" onclick="selectTask(${task.id})" title="Select">&#10003;</button>` : ''}
                    ${isSelected ? `<button class="task-btn cancel" onclick="cancelSelection()" title="Cancel">&#10007;</button>` : ''}
                    <button class="task-btn delete" onclick="deleteTask(${task.id})" title="Delete">&#10005;</button>
                ` : ''}
            </div>
        </div>
    `}).join('');
    
    elements.taskCount.textContent = `${state.tasks.length} task-uri`;
    updateStats({
        completed: state.tasks.filter(t => t.completed).length,
        total: state.tasks.length,
        focusMinutes: state.stats.focusMinutes
    });
}

function addTask() {
    const name = elements.taskName.value.trim();
    const focusDuration = parseInt(elements.focusTime.value) || 25;
    const breakDuration = parseInt(elements.breakTime.value) || 5;
    
    if (!name) {
        showToast('Introdu un nume pentru task!', 'error');
        elements.taskName.focus();
        return;
    }
    
    // Verifică dacă s-a atins limita de task-uri
    if (state.goalLocked && state.tasks.length >= state.dailyGoal) {
        showToast('Ai atins limita de task-uri pentru obiectivul zilnic!', 'error');
        return;
    }
    
    const newTask = {
        id: Date.now(),
        name: name,
        focusDuration: focusDuration,
        breakDuration: breakDuration,
        completed: false,
        active: false,
        started: false  // Flag pentru a ști dacă task-ul a fost pornit
    };
    
    // Send to ESP32
    sendWebSocketMessage({
        action: 'addTask',
        task: newTask
    });
    
    // Update local state
    state.tasks.push(newTask);
    renderTasks();
    
    // Actualizează afișarea plantei (pentru a reflecta noul total de task-uri)
    updatePlant();
    
    // Clear form
    elements.taskName.value = '';
    elements.taskName.focus();
    
    showToast('Task adăugat!', 'success');
}

function startTask(id) {
    const task = state.tasks.find(t => t.id === id);
    if (!task) return;
    
    sendWebSocketMessage({
        action: 'startTask',
        taskId: id
    });
    
    // Update local state
    state.tasks.forEach(t => t.active = false);
    task.active = true;
    task.started = true;  // Marchează ca pornit
    state.status = 'focusing';
    state.currentTask = task.name;
    state.timeLeft = task.focusDuration * 60;
    state.totalTime = task.focusDuration * 60;
    state.selectedTaskId = 0;  // Clear selection
    
    updateStatus({
        state: 'focusing',
        taskName: task.name,
        timeLeft: state.timeLeft,
        totalTime: state.totalTime
    });
    
    renderTasks();
    showToast(`Incepi: ${task.name}`, 'success');
}

// Selectează un task pentru a fi pornit cu flip MPU
function selectTask(id) {
    const task = state.tasks.find(t => t.id === id);
    if (!task) return;
    
    if (task.completed) {
        showToast('Task-ul e deja completat!', 'error');
        return;
    }
    
    if (state.status === 'focusing') {
        showToast('Oprește mai întâi task-ul curent!', 'error');
        return;
    }
    
    sendWebSocketMessage({
        action: 'selectTask',
        taskId: id
    });
    
    // Update local state
    state.selectedTaskId = id;
    task.started = true;
    
    renderTasks();
    showToast(`Task "${task.name}" selectat. Întoarce cubul pentru a începe!`, 'success');
}

// Anulează selecția task-ului
function cancelSelection() {
    state.selectedTaskId = 0;
    renderTasks();
    showToast('Selecție anulată', 'info');
}

function toggleTaskComplete(id) {
    const task = state.tasks.find(t => t.id === id);
    if (!task) return;
    
    // Nu poți bifa un task dacă nu a fost pornit
    if (!task.started) {
        showToast('Trebuie sa pornesti task-ul inainte sa il completezi!', 'error');
        return;
    }
    
    const wasCompleted = task.completed;
    const wasActive = task.active;
    task.completed = !task.completed;
    
    // Calculează timpul efectiv petrecut (dacă task-ul era activ)
    let actualMinutesSpent = 0;
    if (task.completed && wasActive) {
        // Timpul petrecut = totalTime - timeLeft (în secunde), convertit în minute
        const secondsSpent = state.totalTime - state.timeLeft;
        actualMinutesSpent = Math.ceil(secondsSpent / 60); // Rotunjește în sus
        
        task.active = false;
        state.status = 'idle';
        state.timeLeft = 0;
        state.totalTime = 0;
        state.currentTask = null;
        updateStatus({ state: 'idle', taskName: '' });
    } else if (task.completed && !wasActive) {
        // Dacă task-ul nu era activ dar e completat, folosește timpul setat
        actualMinutesSpent = task.focusDuration;
    }
    
    sendWebSocketMessage({
        action: 'toggleTask',
        taskId: id,
        completed: task.completed
    });
    
    // Actualizează pendingWater
    if (task.completed && !wasCompleted) {
        // Task nou completat - adaugă o "udare" disponibilă
        state.pendingWater++;
        state.stats.focusMinutes += actualMinutesSpent;
        showToast('Task completat! Poti uda planta!', 'success');
    } else if (!task.completed && wasCompleted) {
        // Task debifat - scade pendingWater dacă e posibil
        state.pendingWater = Math.max(0, state.pendingWater - 1);
        // Scade și minutele (folosește focusDuration ca estimare)
        state.stats.focusMinutes = Math.max(0, state.stats.focusMinutes - task.focusDuration);
    }
    
    renderTasks();
    updatePlant();
}

function deleteTask(id) {
    showModal(
        'X',
        'Sterge Task',
        'Esti sigur ca vrei sa stergi acest task?',
        () => {
            const task = state.tasks.find(t => t.id === id);
            
            // Dacă task-ul era activ (countdown în desfășurare), resetează timer-ul
            if (task && task.active) {
                state.status = 'idle';
                state.timeLeft = 0;
                state.totalTime = 0;
                state.currentTask = null;
                updateStatus({ state: 'idle', taskName: '' });
            }
            
            // Dacă task-ul era completat, scade pendingWater
            if (task && task.completed && state.pendingWater > 0) {
                state.pendingWater--;
            }
            
            state.tasks = state.tasks.filter(t => t.id !== id);
            
            sendWebSocketMessage({
                action: 'deleteTask',
                taskId: id
            });
            
            renderTasks();
            updatePlant();  // Actualizează afișarea plantei
            showToast('Task sters', 'success');
        }
    );
}

// ============================================
// Plant Actions
// ============================================

function waterPlant() {
    if (state.plant.isWithered) {
        showToast('Planta este ofilită! Folosește lumina pentru a o reînvia.', 'error');
        return;
    }
    
    if (!state.goalLocked || state.dailyGoal <= 0) {
        showToast('Setează mai întâi obiectivul zilnic!', 'error');
        return;
    }
    
    if (state.pendingWater <= 0) {
        showToast('Completează un task pentru a putea uda planta!', 'error');
        return;
    }
    
    if (state.plant.stage >= 3) {
        showToast('Planta a crescut complet! 🌸', 'success');
        return;
    }
    
    sendWebSocketMessage({ action: 'water' });
    
    // Folosește currentSessionGoal dacă există, altfel totalTasks
    const goalsToComplete = state.currentSessionGoal > 0 ? state.currentSessionGoal : state.tasks.length;
    
    // Consumă o "udare" și incrementează wateredCount
    state.pendingWater--;
    state.wateredCount++;
    
    // Stadiile sunt FIX 4: 0 (Semință), 1 (Lăstar), 2 (Creștere), 3 (Înflorit)
    // Logica bazată pe wateredCount (task-uri confirmate prin udare):
    // - 0 udate = Sămânță (0)
    // - 1 udată = Lăstar (1)
    // - 2+ udate, dar NU toate = Creștere (2)
    // - TOATE udate = Înflorit (3)
    let targetStage;
    if (state.wateredCount >= goalsToComplete) {
        targetStage = 3; // Toate udate = Înflorit
    } else if (state.wateredCount >= 2) {
        targetStage = 2; // 2+ dar nu toate = Creștere
    } else if (state.wateredCount >= 1) {
        targetStage = 1; // 1 udată = Lăstar
    } else {
        targetStage = 0; // Nimic udat = Sămânță
    }
    
    const oldStage = state.plant.stage;
    state.plant.stage = targetStage;
    
    const stageInfo = PLANT_STAGES[state.plant.stage];
    updatePlant();
    
    if (state.plant.stage >= 3) {
        showToast('Planta a inflorit complet! Felicitari!', 'success');
    } else if (state.plant.stage > oldStage) {
        showToast(`Planta a crescut! Acum e ${stageInfo.name}!`, 'success');
    } else {
        showToast(`Planta a fost udata! Mai completeaza task-uri pentru a creste!`, 'success');
    }
    
    // Trigger heart animation
    triggerHeartAnimation();
}

function killPlant() {
    showModal(
        'X',
        'Demo Mode',
        'Aceasta va omori planta pentru demonstratie. Poti sa o reinvii cu senzorul de lumina.',
        () => {
            sendWebSocketMessage({ action: 'kill' });
            
            state.plant.isWithered = true;
            updatePlant({ stage: state.plant.stage, isWithered: true });
            updateStatus({ state: 'withered', taskName: 'Planta a murit!' });
            
            showToast('Planta a fost ofilita', 'error');
        }
    );
}

function triggerHeartAnimation() {
    // Visual feedback for water animation
    const heart = document.createElement('div');
    heart.innerHTML = '+';
    heart.style.cssText = `
        position: fixed;
        top: 50%;
        left: 50%;
        font-size: 4rem;
        color: var(--primary);
        font-weight: bold;
        transform: translate(-50%, -50%) scale(0);
        animation: heartPop 0.6s ease forwards;
        z-index: 1000;
        pointer-events: none;
    `;
    document.body.appendChild(heart);
    
    setTimeout(() => heart.remove(), 600);
}

function showReviveAnimation() {
    // Special animation for plant revival
    const reviveOverlay = document.createElement('div');
    reviveOverlay.style.cssText = `
        position: fixed;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        background: rgba(0,0,0,0.9);
        display: flex;
        flex-direction: column;
        align-items: center;
        justify-content: center;
        z-index: 2000;
        animation: fadeIn 0.5s ease;
    `;
    
    reviveOverlay.innerHTML = `
        <div style="font-size: 6rem; animation: bounce 1s ease infinite; text-shadow: 0 0 30px rgba(46,204,113,0.8);">🌱</div>
        <div style="color: #2ecc71; font-size: 2rem; font-weight: bold; margin-top: 1.5rem; text-align: center; text-shadow: 0 2px 10px rgba(0,0,0,0.5);">
            Plant Revived!
        </div>
        <div style="color: white; font-size: 1.2rem; margin-top: 0.8rem; text-align: center; padding: 0 2rem;">
            You can plant again and start a new journey!
        </div>
    `;
    
    document.body.appendChild(reviveOverlay);
    
    // Refresh plant display
    state.plant.stage = 0;
    state.plant.isWithered = false;
    updatePlant();
    
    // Remove after 3 seconds
    setTimeout(() => {
        reviveOverlay.style.animation = 'fadeOut 0.5s ease forwards';
        setTimeout(() => reviveOverlay.remove(), 500);
    }, 3000);
}

// Add heart animation keyframes
const style = document.createElement('style');
style.textContent = `
    @keyframes heartPop {
        0% { transform: translate(-50%, -50%) scale(0); opacity: 1; }
        50% { transform: translate(-50%, -50%) scale(1.5); opacity: 1; }
        100% { transform: translate(-50%, -50%) scale(1); opacity: 0; }
    }
    @keyframes fadeIn {
        from { opacity: 0; }
        to { opacity: 1; }
    }
    @keyframes fadeOut {
        from { opacity: 1; }
        to { opacity: 0; }
    }
    @keyframes bounce {
        0%, 100% { transform: translateY(0); }
        50% { transform: translateY(-20px); }
    }
`;
document.head.appendChild(style);

// ============================================
// UI Helpers
// ============================================

function showToast(message, type = 'success') {
    elements.toastIcon.textContent = type === 'success' ? '+' : '!';
    elements.toastMessage.textContent = message;
    elements.toast.className = `toast ${type} show`;
    
    setTimeout(() => {
        elements.toast.classList.remove('show');
    }, 3000);
}

function showModal(icon, title, message, onConfirm) {
    elements.modalIcon.textContent = icon;
    elements.modalTitle.textContent = title;
    elements.modalMessage.textContent = message;
    elements.modalOverlay.classList.add('show');
    
    // Set up confirm handler
    elements.modalConfirm.onclick = () => {
        hideModal();
        if (onConfirm) onConfirm();
    };
}

function hideModal() {
    elements.modalOverlay.classList.remove('show');
    state.showingConfirmModal = false;
    // Reset modal buttons
    elements.modalConfirm.style.display = '';
    elements.modalCancel.style.display = '';
    elements.modalConfirm.textContent = 'Confirma';
    elements.modalCancel.textContent = 'Anuleaza';
}

// Special modal for flip confirmation
function showFlipConfirmModal() {
    state.showingConfirmModal = true;
    state.flipCancelledWaitingFlipBack = false;
    
    elements.modalIcon.textContent = '?';
    elements.modalTitle.textContent = 'Task Complete?';
    elements.modalMessage.textContent = 'Did you finish the task, or was the flip accidental?';
    elements.modalConfirm.textContent = 'Yes, I finished!';
    elements.modalCancel.textContent = 'Accidental flip';
    elements.modalConfirm.style.display = 'inline-block';
    elements.modalCancel.style.display = 'inline-block';
    elements.modalOverlay.classList.add('show');
    
    // Confirm = task completed
    elements.modalConfirm.onclick = () => {
        hideModal();
        sendWebSocketMessage({ action: 'confirmComplete' });
        showToast('Congratulations! Task completed!', 'success');
        state.selectedTaskId = 0;
        renderTasks();
    };
    
    // Cancel = accidental flip, need to flip back
    elements.modalCancel.onclick = () => {
        state.flipCancelledWaitingFlipBack = true;
        elements.modalIcon.textContent = '↻';
        elements.modalTitle.textContent = 'Flip back to resume';
        elements.modalMessage.textContent = 'Flip the cube face-down to continue your focus session.';
        elements.modalConfirm.style.display = 'none';
        elements.modalCancel.style.display = 'none';
        sendWebSocketMessage({ action: 'cancelComplete' });
    };
}

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

function updateClock() {
    const now = new Date();
    const hours = String(now.getHours()).padStart(2, '0');
    const minutes = String(now.getMinutes()).padStart(2, '0');
    elements.currentTime.textContent = `${hours}:${minutes}`;
}

// ============================================
// Timer Logic (local countdown for smooth UI)
// Only runs when WebSocket is disconnected
// ============================================

function startLocalTimer() {
    setInterval(() => {
        // Only run local timer if WebSocket is NOT connected
        // When connected, ESP32 sends updates and we use those
        if (!state.wsConnected && state.timeLeft > 0 && (state.status === 'focusing' || state.status === 'break')) {
            state.timeLeft--;
            updateTimerDisplay();
            
            if (state.timeLeft === 0) {
                handleTimerComplete();
            }
        }
    }, 1000);
}

function handleTimerComplete() {
    if (state.status === 'focusing') {
        // Găsește task-ul activ pentru a lua durata de pauză
        const activeTask = state.tasks.find(t => t.active);
        const breakDuration = activeTask ? activeTask.breakDuration : 5;
        
        showToast('Focus terminat! Ia o pauza!', 'success');
        
        // Treci la starea de break cu countdown
        state.status = 'break';
        state.timeLeft = breakDuration * 60;
        state.totalTime = breakDuration * 60;
        
        updateStatus({
            state: 'break',
            taskName: 'Pauza',
            timeLeft: state.timeLeft,
            totalTime: state.totalTime
        });
    } else if (state.status === 'break') {
        // Pauza terminată - reîncepe ciclul de focus
        const activeTask = state.tasks.find(t => t.active);
        
        if (activeTask) {
            // Continuă cu focus pentru același task
            state.status = 'focusing';
            state.timeLeft = activeTask.focusDuration * 60;
            state.totalTime = activeTask.focusDuration * 60;
            state.currentTask = activeTask.name;
            
            updateStatus({
                state: 'focusing',
                taskName: activeTask.name,
                timeLeft: state.timeLeft,
                totalTime: state.totalTime
            });
            
            showToast('Pauza terminata! Continua cu focus!', 'success');
        } else {
            // Dacă nu mai există task activ, revino la idle
            state.status = 'idle';
            state.timeLeft = 0;
            state.currentTask = null;
            
            updateStatus({ state: 'idle', taskName: '' });
            renderTasks();
        }
    }
}

// ============================================
// Weekly Statistics
// ============================================

async function fetchWeeklyStats() {
    if (isDevMode()) {
        updateWeeklyStats({
            weekly: { totalTasks: 0, totalFocus: 0, avgTasksPerDay: 0, mostProductiveDayName: '-' },
            days: []
        });
        return;
    }
    
    try {
        const response = await fetch(`${CONFIG.API_BASE}/api/stats`);
        const data = await response.json();
        updateWeeklyStats(data);
    } catch (e) {
        console.error('Failed to fetch weekly stats:', e);
    }
}

function updateWeeklyStats(data) {
    if (!data) return;
    
    // Update TODAY's stats from Analytics (accurate from ESP32)
    if (data.todayTasks !== undefined || data.todayFocus !== undefined) {
        state.stats.completed = data.todayTasks || 0;
        state.stats.focusMinutes = data.todayFocus || 0;
        
        // Update the display
        elements.tasksCompleted.textContent = state.stats.completed;
        elements.focusTotal.textContent = state.stats.focusMinutes;
        
        // Update progress bar based on tasks
        const total = state.tasks.length || state.stats.total || 1;
        const percent = Math.round((state.stats.completed / total) * 100);
        elements.progressPercent.textContent = `${percent}%`;
        elements.progressFill.style.width = `${percent}%`;
    }
    
    if (!data.weekly) return;
    
    const weekly = data.weekly;
    
    // Update weekly summary numbers
    if (elements.weeklyTasks) elements.weeklyTasks.textContent = weekly.totalTasks || 0;
    if (elements.weeklyFocus) elements.weeklyFocus.textContent = weekly.totalFocus || 0;
    if (elements.weeklyAvg) elements.weeklyAvg.textContent = (weekly.avgTasksPerDay || 0).toFixed(1);
    if (elements.bestDay) elements.bestDay.textContent = weekly.mostProductiveDayName || '-';
    
    // Render weekly chart
    if (elements.weeklyChart && data.days) {
        renderWeeklyChart(data.days);
    }
}

function renderWeeklyChart(days) {
    if (!elements.weeklyChart) return;
    
    const dayNames = ['Dum', 'Lun', 'Mar', 'Mie', 'Joi', 'Vin', 'Sam'];
    const today = new Date().getDay();
    
    // Find max value for scaling
    const maxTasks = Math.max(...days.map(d => d.tasks || 0), 1);
    
    let html = '';
    for (let i = 6; i >= 0; i--) {
        const dayData = days.find(d => d.daysAgo === i) || { tasks: 0, focus: 0, valid: false };
        const height = dayData.valid ? Math.max((dayData.tasks / maxTasks) * 100, 8) : 8;
        const dayIndex = (today - i + 7) % 7;
        const isToday = i === 0;
        
        html += `<div class="chart-bar ${isToday ? 'today' : ''} ${dayData.valid ? 'has-data' : ''}" 
                      style="height: ${height}%" 
                      data-day="${dayNames[dayIndex]}"
                      onclick="showDayDetails(${i}, ${dayData.tasks || 0}, ${dayData.focus || 0}, '${dayNames[dayIndex]}')"
                      title="${dayData.tasks || 0} task-uri, ${dayData.focus || 0} min focus"></div>`;
    }
    
    elements.weeklyChart.innerHTML = html;
}

function showDayDetails(daysAgo, tasks, focus, dayName) {
    const label = daysAgo === 0 ? 'Azi' : (daysAgo === 1 ? 'Ieri' : dayName);
    const hours = Math.floor(focus / 60);
    const mins = focus % 60;
    const focusStr = hours > 0 ? `${hours}h ${mins}m` : `${mins} min`;
    
    showToast(`${label}: ${tasks} task-uri, ${focusStr} focus`, 'success');
}

// ============================================
// Sync Time from Phone
// ============================================

function syncTimeFromPhone() {
    const now = new Date();
    const hours = now.getHours();
    const minutes = now.getMinutes();
    const seconds = now.getSeconds();
    const day = now.getDate();
    const month = now.getMonth() + 1; // 0-indexed
    const year = now.getFullYear();
    
    sendWebSocketMessage({
        action: 'setTime',
        hours: hours,
        minutes: minutes,
        seconds: seconds,
        day: day,
        month: month,
        year: year
    });
    
    showToast(`Ora sincronizata: ${String(hours).padStart(2, '0')}:${String(minutes).padStart(2, '0')}`, 'success');
}

// ============================================
// Reset Day
// ============================================

function resetDay() {
    showModal(
        '⚠️',
        'Resetare Zi',
        'Aceasta va reseta toate task-urile si planta pentru o zi noua. Esti sigur?',
        () => {
            sendWebSocketMessage({ action: 'restartDay' });
            
            // Reset local state
            state.tasks = [];
            state.plant.stage = 0;
            state.pendingWater = 0;
            state.wateredCount = 0;
            state.dailyGoal = 0;
            state.goalLocked = false;
            state.status = 'idle';
            state.timeLeft = 0;
            
            renderTasks();
            updatePlant();
            updateStatus({ state: 'idle', taskName: '' });
            
            showToast('Zi resetata! Incepe o zi noua!', 'success');
        }
    );
}

// ============================================
// Mock Data (pentru dezvoltare fără ESP32)
// ============================================

function loadMockData() {
    // Start fresh - no predefined tasks
    state.tasks = [];
    
    state.plant.stage = 0;
    state.plant.isWithered = false;
    state.pendingWater = 0;
    state.wateredCount = 0;
    state.dailyGoal = 0;
    state.currentSessionGoal = 0;
    state.goalLocked = false;
    state.status = 'idle';
    state.timeLeft = 0;
    state.totalTime = 0;
    state.currentTask = null;
    
    state.stats = {
        completed: 0,
        total: 0,
        focusMinutes: 0
    };
    
    // Set default goal value in input
    if (elements.dailyGoalInput) {
        elements.dailyGoalInput.value = 3;
    }
    
    // Dezactivează explicit butonul de udare
    if (elements.btnWater) {
        elements.btnWater.disabled = true;
    }
    
    renderTasks();
    updatePlant();
    updateStats(state.stats);
    updateStatus({ state: 'idle', taskName: null, timeLeft: 0, totalTime: 0 });
    
    elements.espIP.textContent = 'Dev Mode';
}

function handleMockAction(action) {
    console.log('Mock action:', action);
    // Simulate responses locally
    return { success: true };
}

// ============================================
// Event Listeners
// ============================================

function initEventListeners() {
    // Add task
    elements.btnAddTask.addEventListener('click', addTask);
    elements.taskName.addEventListener('keypress', (e) => {
        if (e.key === 'Enter') addTask();
    });
    
    // Daily goal
    if (elements.btnSetGoal) {
        elements.btnSetGoal.addEventListener('click', setDailyGoal);
    }
    if (elements.dailyGoalInput) {
        elements.dailyGoalInput.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') setDailyGoal();
        });
    }
    
    // Plant actions
    elements.btnWater.addEventListener('click', waterPlant);
    elements.btnKill.addEventListener('click', killPlant);
    
    // Sync time button
    if (elements.btnSyncTime) {
        elements.btnSyncTime.addEventListener('click', syncTimeFromPhone);
    }
    
    // Reset day button
    if (elements.btnResetDay) {
        elements.btnResetDay.addEventListener('click', resetDay);
    }
    
    // Modal - only allow closing by clicking overlay if NOT waiting for flip back
    // Note: modalCancel behavior is set dynamically in showModal/showFlipConfirmModal
    elements.modalOverlay.addEventListener('click', (e) => {
        // Only close if clicking directly on overlay AND not waiting for flip back
        if (e.target === elements.modalOverlay && !state.flipCancelledWaitingFlipBack) {
            hideModal();
        }
    });
    
    // Prevent zoom on double-tap (mobile)
    document.addEventListener('touchend', (e) => {
        const now = Date.now();
        if (now - (window.lastTouchEnd || 0) < 300) {
            e.preventDefault();
        }
        window.lastTouchEnd = now;
    }, false);
}

// ============================================
// Initialization
// ============================================

function init() {
    console.log('🌱 Productivity Bloom initializing...');
    
    // Initialize event listeners
    initEventListeners();
    
    // Start clock update
    updateClock();
    setInterval(updateClock, 1000);
    
    // Try WebSocket first
    connectWebSocket();
    
    // Start local timer (only runs when WebSocket disconnected)
    startLocalTimer();
    
    // Initial data fetch - do this immediately!
    console.log('Fetching initial status...');
    fetchStatus();
    fetchTasks();
    fetchWeeklyStats();
    
    // Refresh weekly stats every 30 seconds
    setInterval(fetchWeeklyStats, 30000);
    
    // API polling only when WebSocket is not connected
    // Reduces load when already getting updates via WebSocket
    setInterval(() => {
        if (!state.wsConnected) {
            fetchStatus();
            console.log('Polling for status (WS disconnected)...');
        }
    }, 2000);  // Poll every 2 seconds instead of 1
    
    console.log('🌱 Productivity Bloom ready!');
}

// Start app when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
} else {
    init();
}
