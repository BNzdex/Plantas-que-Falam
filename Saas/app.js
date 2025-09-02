// Configurações da aplicação
const CONFIG = {
    API_BASE_URL: '/api/sensor',
    REFRESH_INTERVAL: 2000,
    CHART_MAX_POINTS: 20
};

// Estado global da aplicação
let appState = {
    currentTab: 'dashboard',
    refreshInterval: null,
    charts: {
        magnitude: null,
        bands: null
    },
    isConnected: false,
    refreshRate: 2000
};

// Inicialização da aplicação
document.addEventListener('DOMContentLoaded', function() {
    initializeApp();
});

function initializeApp() {
    setupNavigation();
    setupSettings();
    startDataRefresh();
    loadInitialData();
    
    console.log('🌱 Plantas que Falam - Aplicação iniciada');
}

// Configuração da navegação
function setupNavigation() {
    const navButtons = document.querySelectorAll('.nav-btn');
    
    navButtons.forEach(button => {
        button.addEventListener('click', function() {
            const tabId = this.getAttribute('data-tab');
            switchTab(tabId);
        });
    });
}

function switchTab(tabId) {
    // Atualizar botões de navegação
    document.querySelectorAll('.nav-btn').forEach(btn => {
        btn.classList.remove('active');
    });
    document.querySelector(`[data-tab="${tabId}"]`).classList.add('active');
    
    // Atualizar conteúdo das abas
    document.querySelectorAll('.tab-content').forEach(tab => {
        tab.classList.remove('active');
    });
    document.getElementById(`${tabId}-tab`).classList.add('active');
    
    appState.currentTab = tabId;
    
    // Carregar dados específicos da aba
    if (tabId === 'plants') {
        loadPlantsData();
    } else if (tabId === 'analytics') {
        loadAnalyticsData();
    }
}

// Configuração das configurações
function setupSettings() {
    const refreshRateSelect = document.getElementById('refresh-rate');
    const themeSelect = document.getElementById('theme-select');
    const notificationsToggle = document.getElementById('notifications-toggle');
    
    if (refreshRateSelect) {
        refreshRateSelect.addEventListener('change', function() {
            appState.refreshRate = parseInt(this.value);
            restartDataRefresh();
        });
    }
    
    if (themeSelect) {
        themeSelect.addEventListener('change', function() {
            changeTheme(this.value);
        });
    }
    
    if (notificationsToggle) {
        notificationsToggle.addEventListener('change', function() {
            if (this.checked && 'Notification' in window) {
                Notification.requestPermission();
            }
        });
    }
}

function changeTheme(theme) {
    document.body.className = `${theme}-theme`;
    localStorage.setItem('theme', theme);
}

// Gerenciamento de dados
function startDataRefresh() {
    if (appState.refreshInterval) {
        clearInterval(appState.refreshInterval);
    }
    
    appState.refreshInterval = setInterval(() => {
        if (appState.currentTab === 'dashboard') {
            loadSensorData();
        }
    }, appState.refreshRate);
}

function restartDataRefresh() {
    startDataRefresh();
    console.log(`🔄 Intervalo de atualização alterado para ${appState.refreshRate}ms`);
}

function loadInitialData() {
    loadSensorData();
}

// Carregamento de dados do sensor
async function loadSensorData() {
    try {
        updateConnectionStatus('connecting');
        
        const response = await fetch(`${CONFIG.API_BASE_URL}/data`);
        
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        
        const data = await response.json();
        updateDashboard(data);
        updateConnectionStatus('online');
        
    } catch (error) {
        console.error('Erro ao carregar dados do sensor:', error);
        updateConnectionStatus('offline');
    }
}

function updateDashboard(data) {
    // Atualizar métricas principais
    updateElement('raw-value', data.raw_value);
    updateElement('voltage', `${data.voltage} V`);
    updateElement('dominant-magnitude', data.dominant_magnitude.toFixed(3));
    updateElement('dominant-magnitude-db', `${data.dominant_magnitude_db.toFixed(1)} dB`);
    updateElement('average-magnitude', data.average_magnitude.toFixed(3));
    
    // Atualizar frequência dominante
    const freqValue = data.dominant_frequency < 1000 
        ? `${data.dominant_frequency.toFixed(1)} Hz`
        : `${(data.dominant_frequency / 1000).toFixed(2)} kHz`;
    updateElement('dominant-freq', freqValue);
    
    // Atualizar status da planta
    const plantStatus = data.status === 'online' ? '🟢 Planta Falando' : '🔴 Silêncio';
    updateElement('plant-status', plantStatus);
    
    // Atualizar gráficos
    updateMagnitudeChart(data.history);
    updateBandsChart(data.bands);
    
    // Atualizar lista de bandas
    updateBandsList(data.bands);
    
    // Verificar notificações
    checkNotifications(data);
}

function updateElement(id, value) {
    const element = document.getElementById(id);
    if (element) {
        element.textContent = value;
    }
}

function updateConnectionStatus(status) {
    const statusDot = document.getElementById('status-dot');
    const statusText = document.getElementById('connection-status');
    
    if (statusDot && statusText) {
        statusDot.className = `status-dot ${status}`;
        
        switch (status) {
            case 'online':
                statusText.textContent = 'Online';
                appState.isConnected = true;
                break;
            case 'offline':
                statusText.textContent = 'Offline';
                appState.isConnected = false;
                break;
            case 'connecting':
                statusText.textContent = 'Conectando...';
                break;
        }
    }
}

// Gráficos
function updateMagnitudeChart(historyData) {
    const ctx = document.getElementById('magnitudeChart');
    if (!ctx) return;
    
    if (appState.charts.magnitude) {
        // Atualizar gráfico existente
        appState.charts.magnitude.data.labels = historyData.map(item => item.time);
        appState.charts.magnitude.data.datasets[0].data = historyData.map(item => item.magnitude);
        appState.charts.magnitude.update('none');
    } else {
        // Criar novo gráfico
        appState.charts.magnitude = new Chart(ctx, {
            type: 'line',
            data: {
                labels: historyData.map(item => item.time),
                datasets: [{
                    label: 'Magnitude da Comunicação',
                    data: historyData.map(item => item.magnitude),
                    borderColor: '#22c55e',
                    backgroundColor: 'rgba(34, 197, 94, 0.1)',
                    borderWidth: 2,
                    fill: true,
                    tension: 0.4,
                    pointBackgroundColor: '#22c55e',
                    pointBorderColor: '#16a34a',
                    pointRadius: 4,
                    pointHoverRadius: 6
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        display: true,
                        labels: {
                            color: 'white',
                            font: {
                                size: 12
                            }
                        }
                    },
                    tooltip: {
                        backgroundColor: 'rgba(0, 0, 0, 0.8)',
                        titleColor: 'white',
                        bodyColor: 'white',
                        borderColor: '#22c55e',
                        borderWidth: 1
                    }
                },
                scales: {
                    x: {
                        display: true,
                        grid: {
                            color: 'rgba(255, 255, 255, 0.1)'
                        },
                        ticks: {
                            color: 'rgba(255, 255, 255, 0.7)'
                        }
                    },
                    y: {
                        display: true,
                        beginAtZero: true,
                        grid: {
                            color: 'rgba(255, 255, 255, 0.1)'
                        },
                        ticks: {
                            color: 'rgba(255, 255, 255, 0.7)'
                        }
                    }
                },
                animation: {
                    duration: 750,
                    easing: 'easeInOutQuart'
                }
            }
        });
    }
}

function updateBandsChart(bandsData) {
    const ctx = document.getElementById('bandsChart');
    if (!ctx) return;
    
    if (appState.charts.bands) {
        // Atualizar gráfico existente
        appState.charts.bands.data.labels = bandsData.map(band => band.name);
        appState.charts.bands.data.datasets[0].data = bandsData.map(band => band.magnitude_db);
        appState.charts.bands.data.datasets[0].backgroundColor = bandsData.map(band => band.color);
        appState.charts.bands.update('none');
    } else {
        // Criar novo gráfico
        appState.charts.bands = new Chart(ctx, {
            type: 'bar',
            data: {
                labels: bandsData.map(band => band.name),
                datasets: [{
                    label: 'Magnitude (dB)',
                    data: bandsData.map(band => band.magnitude_db),
                    backgroundColor: bandsData.map(band => band.color),
                    borderColor: bandsData.map(band => band.color),
                    borderWidth: 1,
                    borderRadius: 4,
                    borderSkipped: false
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        display: false
                    },
                    tooltip: {
                        backgroundColor: 'rgba(0, 0, 0, 0.8)',
                        titleColor: 'white',
                        bodyColor: 'white',
                        borderColor: '#22c55e',
                        borderWidth: 1,
                        callbacks: {
                            afterLabel: function(context) {
                                const band = bandsData[context.dataIndex];
                                return `Faixa: ${band.range}`;
                            }
                        }
                    }
                },
                scales: {
                    x: {
                        display: true,
                        grid: {
                            color: 'rgba(255, 255, 255, 0.1)'
                        },
                        ticks: {
                            color: 'rgba(255, 255, 255, 0.7)',
                            maxRotation: 45
                        }
                    },
                    y: {
                        display: true,
                        grid: {
                            color: 'rgba(255, 255, 255, 0.1)'
                        },
                        ticks: {
                            color: 'rgba(255, 255, 255, 0.7)'
                        }
                    }
                },
                animation: {
                    duration: 750,
                    easing: 'easeInOutQuart'
                }
            }
        });
    }
}

function updateBandsList(bandsData) {
    const bandsList = document.getElementById('bands-list');
    if (!bandsList) return;
    
    bandsList.innerHTML = '';
    
    bandsData.forEach(band => {
        const bandItem = document.createElement('div');
        bandItem.className = 'band-item';
        
        bandItem.innerHTML = `
            <div class="band-info">
                <div class="band-name">${band.name}</div>
                <div class="band-range">${band.range}</div>
            </div>
            <div class="band-value">
                <div class="band-magnitude">${band.magnitude_db.toFixed(1)} dB</div>
                <div class="band-color" style="background-color: ${band.color}"></div>
            </div>
        `;
        
        bandsList.appendChild(bandItem);
    });
}

// Carregamento de dados das plantas
async function loadPlantsData() {
    try {
        const plantsGrid = document.getElementById('plants-grid');
        if (!plantsGrid) return;
        
        plantsGrid.innerHTML = '<div class="loading">Carregando plantas...</div>';
        
        const response = await fetch(`${CONFIG.API_BASE_URL}/plants`);
        
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        
        const plants = await response.json();
        displayPlants(plants);
        
    } catch (error) {
        console.error('Erro ao carregar dados das plantas:', error);
        const plantsGrid = document.getElementById('plants-grid');
        if (plantsGrid) {
            plantsGrid.innerHTML = '<div class="loading">Erro ao carregar plantas</div>';
        }
    }
}

function displayPlants(plants) {
    const plantsGrid = document.getElementById('plants-grid');
    if (!plantsGrid) return;
    
    plantsGrid.innerHTML = '';
    
    plants.forEach(plant => {
        const plantCard = document.createElement('div');
        plantCard.className = 'plant-card';
        
        plantCard.innerHTML = `
            <div class="plant-header">
                <div class="plant-name">${plant.name}</div>
                <div class="plant-status-badge ${plant.status}">${plant.status === 'online' ? 'Online' : 'Offline'}</div>
            </div>
            <div class="plant-info">
                <div class="plant-type">${plant.type}</div>
                <div class="plant-location">📍 ${plant.location}</div>
            </div>
            <div class="plant-metrics">
                <div class="plant-metric">
                    <div class="plant-metric-value">${plant.communication_frequency.toFixed(1)} Hz</div>
                    <div class="plant-metric-label">Frequência</div>
                </div>
                <div class="plant-metric">
                    <div class="plant-metric-value">${plant.health_score}%</div>
                    <div class="plant-metric-label">Saúde</div>
                </div>
                <div class="plant-metric">
                    <div class="plant-metric-value">${formatDate(plant.last_communication)}</div>
                    <div class="plant-metric-label">Última comunicação</div>
                </div>
            </div>
        `;
        
        plantsGrid.appendChild(plantCard);
    });
}

// Carregamento de dados de analytics
async function loadAnalyticsData() {
    try {
        const analyticsContent = document.getElementById('analytics-content');
        if (!analyticsContent) return;
        
        analyticsContent.innerHTML = '<div class="loading">Carregando analytics...</div>';
        
        const response = await fetch(`${CONFIG.API_BASE_URL}/analytics/summary`);
        
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        
        const analytics = await response.json();
        displayAnalytics(analytics);
        
    } catch (error) {
        console.error('Erro ao carregar dados de analytics:', error);
        const analyticsContent = document.getElementById('analytics-content');
        if (analyticsContent) {
            analyticsContent.innerHTML = '<div class="loading">Erro ao carregar analytics</div>';
        }
    }
}

function displayAnalytics(analytics) {
    const analyticsContent = document.getElementById('analytics-content');
    if (!analyticsContent) return;
    
    analyticsContent.innerHTML = `
        <div class="analytics-summary">
            <div class="summary-card">
                <div class="summary-value">${analytics.total_plants}</div>
                <div class="summary-label">Total de Plantas</div>
            </div>
            <div class="summary-card">
                <div class="summary-value">${analytics.active_plants}</div>
                <div class="summary-label">Plantas Ativas</div>
            </div>
            <div class="summary-card">
                <div class="summary-value">${analytics.total_communications_today}</div>
                <div class="summary-label">Comunicações Hoje</div>
            </div>
            <div class="summary-card">
                <div class="summary-value">${analytics.average_frequency.toFixed(1)} Hz</div>
                <div class="summary-label">Frequência Média</div>
            </div>
        </div>
        
        <div class="chart-card">
            <div class="chart-header">
                <h3>📊 Tendências de Comunicação (24h)</h3>
                <p>Atividade de comunicação ao longo do dia</p>
            </div>
            <div class="chart-container">
                <canvas id="trendsChart"></canvas>
            </div>
        </div>
        
        <div class="chart-card">
            <div class="chart-header">
                <h3>🎯 Distribuição de Frequências</h3>
                <p>Percentual de comunicações por faixa de frequência</p>
            </div>
            <div class="chart-container">
                <canvas id="distributionChart"></canvas>
            </div>
        </div>
    `;
    
    // Criar gráficos de analytics
    createTrendsChart(analytics.communication_trends);
    createDistributionChart(analytics.frequency_distribution);
}

function createTrendsChart(trendsData) {
    const ctx = document.getElementById('trendsChart');
    if (!ctx) return;
    
    new Chart(ctx, {
        type: 'line',
        data: {
            labels: trendsData.map(item => item.hour),
            datasets: [{
                label: 'Comunicações por Hora',
                data: trendsData.map(item => item.count),
                borderColor: '#22c55e',
                backgroundColor: 'rgba(34, 197, 94, 0.1)',
                borderWidth: 2,
                fill: true,
                tension: 0.4
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    labels: { color: 'white' }
                }
            },
            scales: {
                x: {
                    grid: { color: 'rgba(255, 255, 255, 0.1)' },
                    ticks: { color: 'rgba(255, 255, 255, 0.7)' }
                },
                y: {
                    grid: { color: 'rgba(255, 255, 255, 0.1)' },
                    ticks: { color: 'rgba(255, 255, 255, 0.7)' }
                }
            }
        }
    });
}

function createDistributionChart(distributionData) {
    const ctx = document.getElementById('distributionChart');
    if (!ctx) return;
    
    new Chart(ctx, {
        type: 'doughnut',
        data: {
            labels: distributionData.map(item => item.range),
            datasets: [{
                data: distributionData.map(item => item.percentage),
                backgroundColor: [
                    '#22c55e',
                    '#16a34a',
                    '#15803d',
                    '#166534'
                ],
                borderWidth: 2,
                borderColor: 'rgba(255, 255, 255, 0.1)'
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    position: 'bottom',
                    labels: { 
                        color: 'white',
                        padding: 20
                    }
                },
                tooltip: {
                    callbacks: {
                        label: function(context) {
                            return `${context.label}: ${context.parsed}%`;
                        }
                    }
                }
            }
        }
    });
}

// Notificações
function checkNotifications(data) {
    const notificationsEnabled = document.getElementById('notifications-toggle')?.checked;
    
    if (notificationsEnabled && 'Notification' in window && Notification.permission === 'granted') {
        // Notificar quando a planta começar a "falar"
        if (data.status === 'online' && data.dominant_magnitude > 0.1) {
            const lastNotification = localStorage.getItem('lastNotification');
            const now = Date.now();
            
            // Evitar spam de notificações (mínimo 30 segundos entre notificações)
            if (!lastNotification || (now - parseInt(lastNotification)) > 30000) {
                new Notification('🌱 Sua planta está falando!', {
                    body: `${data.plant_name} está se comunicando em ${data.dominant_frequency.toFixed(1)} Hz`,
                    icon: '/favicon.ico'
                });
                
                localStorage.setItem('lastNotification', now.toString());
            }
        }
    }
}

// Utilitários
function formatDate(dateString) {
    const date = new Date(dateString);
    const now = new Date();
    const diffMs = now - date;
    const diffMins = Math.floor(diffMs / 60000);
    
    if (diffMins < 1) return 'Agora';
    if (diffMins < 60) return `${diffMins}m`;
    if (diffMins < 1440) return `${Math.floor(diffMins / 60)}h`;
    return `${Math.floor(diffMins / 1440)}d`;
}

// Limpeza ao sair da página
window.addEventListener('beforeunload', function() {
    if (appState.refreshInterval) {
        clearInterval(appState.refreshInterval);
    }
});

// Tratamento de erros globais
window.addEventListener('error', function(event) {
    console.error('Erro na aplicação:', event.error);
});

// Log de inicialização
console.log('🌿 Plantas que Falam - JavaScript carregado');
console.log('📡 API Base URL:', CONFIG.API_BASE_URL);
console.log('⏱️ Intervalo de atualização:', CONFIG.REFRESH_INTERVAL + 'ms');

