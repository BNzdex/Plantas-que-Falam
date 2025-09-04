#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "arduinoFFT.h"

// Configuração do Display OLED (LilyGO)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Configurações WiFi
const char* ssid = "iPhone"; 
const char* password = "123456789"; 

// Configurações FFT
#define SAMPLES 512              
#define SAMPLING_FREQUENCY 10000 
#define PIEZO_PIN 34            

// Variáveis FFT
double vReal[SAMPLES];
double vImag[SAMPLES];
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, SAMPLING_FREQUENCY);

// Servidor Web
WebServer server(80);


const char* HTML_CONTENT = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Plantas que Falam - Dashboard SaaS</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
/* Reset e Base */
* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}

body {
    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    background: linear-gradient(135deg, #0f2027 0%, #203a43 50%, #2c5364 100%);
    color: white;
    min-height: 100vh;
    overflow-x: hidden;
}

/* Tema Verde */
body.green-theme {
    background: linear-gradient(135deg, #134e5e 0%, #71b280 100%);
}

body.dark-theme {
    background: linear-gradient(135deg, #1a1a1a 0%, #2d2d2d 100%);
}

body.light-theme {
    background: linear-gradient(135deg, #f5f7fa 0%, #c3cfe2 100%);
    color: #333;
}

.container {
    max-width: 1400px;
    margin: 0 auto;
    padding: 0 20px;
}

/* Header */
.header {
    background: rgba(0, 0, 0, 0.2);
    backdrop-filter: blur(10px);
    border-bottom: 1px solid rgba(255, 255, 255, 0.1);
    padding: 1rem 0;
    position: sticky;
    top: 0;
    z-index: 100;
}

.header-content {
    display: flex;
    justify-content: space-between;
    align-items: center;
    flex-wrap: wrap;
    gap: 1rem;
}

.logo {
    display: flex;
    align-items: center;
    gap: 0.5rem;
}

.logo-icon {
    font-size: 2rem;
}

.logo h1 {
    font-size: 1.5rem;
    font-weight: bold;
}

.nav {
    display: flex;
    gap: 0.5rem;
    flex-wrap: wrap;
}

.nav-btn {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    padding: 0.5rem 1rem;
    background: transparent;
    border: 1px solid rgba(255, 255, 255, 0.2);
    border-radius: 8px;
    color: white;
    cursor: pointer;
    transition: all 0.3s ease;
    font-size: 0.9rem;
}

.nav-btn:hover {
    background: rgba(34, 197, 94, 0.2);
    border-color: rgba(34, 197, 94, 0.5);
}

.nav-btn.active {
    background: rgba(34, 197, 94, 0.3);
    border-color: rgba(34, 197, 94, 0.7);
}

.nav-icon {
    font-size: 1rem;
}

.status-indicator {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    padding: 0.5rem 1rem;
    background: rgba(0, 0, 0, 0.2);
    border-radius: 20px;
    font-size: 0.9rem;
}

.status-dot {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: #ef4444;
    animation: pulse 2s infinite;
}

.status-dot.online {
    background: #22c55e;
}

@keyframes pulse {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.5; }
}

/* Main Content */
.main-content {
    padding: 2rem 0;
}

.tab-content {
    display: none;
}

.tab-content.active {
    display: block;
}

/* Status Header */
.status-header {
    background: rgba(0, 0, 0, 0.2);
    backdrop-filter: blur(10px);
    border-radius: 15px;
    padding: 2rem;
    margin-bottom: 2rem;
    display: flex;
    justify-content: space-between;
    align-items: center;
    flex-wrap: wrap;
    gap: 1rem;
    border: 1px solid rgba(255, 255, 255, 0.1);
}

.status-info h2 {
    font-size: 2rem;
    margin-bottom: 0.5rem;
}

.status-info p {
    color: rgba(255, 255, 255, 0.8);
    font-size: 1.1rem;
}

.frequency-display {
    text-align: center;
    padding: 1rem;
    background: linear-gradient(45deg, #22c55e, #16a34a);
    border-radius: 15px;
    min-width: 200px;
}

.frequency-value {
    font-size: 2rem;
    font-weight: bold;
    margin-bottom: 0.5rem;
}

.frequency-label {
    font-size: 0.9rem;
    opacity: 0.9;
    margin-bottom: 0.5rem;
}

.plant-status {
    font-size: 1rem;
    font-weight: bold;
}

/* Metrics Grid */
.metrics-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
    gap: 1.5rem;
    margin-bottom: 2rem;
}

.metric-card {
    background: rgba(0, 0, 0, 0.2);
    backdrop-filter: blur(10px);
    border-radius: 15px;
    padding: 1.5rem;
    border: 1px solid rgba(255, 255, 255, 0.1);
    transition: transform 0.3s ease;
}

.metric-card:hover {
    transform: translateY(-5px);
}

.metric-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 1rem;
}

.metric-title {
    font-size: 0.9rem;
    color: rgba(255, 255, 255, 0.8);
}

.metric-icon {
    font-size: 1.2rem;
}

.metric-value {
    font-size: 2rem;
    font-weight: bold;
    margin-bottom: 0.5rem;
    color: #22c55e;
}

.metric-subtitle {
    font-size: 0.8rem;
    color: rgba(255, 255, 255, 0.6);
}

/* Charts Grid */
.charts-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(400px, 1fr));
    gap: 2rem;
    margin-bottom: 2rem;
}

.chart-card {
    background: rgba(0, 0, 0, 0.2);
    backdrop-filter: blur(10px);
    border-radius: 15px;
    padding: 1.5rem;
    border: 1px solid rgba(255, 255, 255, 0.1);
}

.chart-header {
    margin-bottom: 1rem;
}

.chart-header h3 {
    font-size: 1.2rem;
    margin-bottom: 0.5rem;
    color: #22c55e;
}

.chart-header p {
    font-size: 0.9rem;
    color: rgba(255, 255, 255, 0.7);
}

.chart-container {
    position: relative;
    height: 300px;
    width: 100%;
}

/* Bands Card */
.bands-card {
    background: rgba(0, 0, 0, 0.2);
    backdrop-filter: blur(10px);
    border-radius: 15px;
    padding: 1.5rem;
    border: 1px solid rgba(255, 255, 255, 0.1);
    margin-bottom: 2rem;
}

.bands-header {
    margin-bottom: 1.5rem;
}

.bands-header h3 {
    font-size: 1.2rem;
    margin-bottom: 0.5rem;
    color: #22c55e;
}

.bands-header p {
    font-size: 0.9rem;
    color: rgba(255, 255, 255, 0.7);
}

.bands-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    gap: 1rem;
}

.band-item {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 1rem;
    background: rgba(255, 255, 255, 0.05);
    border-radius: 10px;
    border: 1px solid rgba(255, 255, 255, 0.1);
}

.band-info {
    flex: 1;
}

.band-name {
    font-weight: bold;
    margin-bottom: 0.25rem;
}

.band-range {
    font-size: 0.8rem;
    color: rgba(255, 255, 255, 0.6);
}

.band-value {
    text-align: right;
    display: flex;
    align-items: center;
    gap: 0.5rem;
}

.band-magnitude {
    font-weight: bold;
    color: #22c55e;
}

.band-color {
    width: 12px;
    height: 12px;
    border-radius: 50%;
}

/* Tab Headers */
.tab-header {
    text-align: center;
    margin-bottom: 2rem;
}

.tab-header h2 {
    font-size: 2rem;
    margin-bottom: 0.5rem;
}

.tab-header p {
    color: rgba(255, 255, 255, 0.8);
    font-size: 1.1rem;
}

/* Plants Grid */
.plants-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    gap: 1.5rem;
}

.plant-card {
    background: rgba(0, 0, 0, 0.2);
    backdrop-filter: blur(10px);
    border-radius: 15px;
    padding: 1.5rem;
    border: 1px solid rgba(255, 255, 255, 0.1);
    transition: transform 0.3s ease;
}

.plant-card:hover {
    transform: translateY(-5px);
}

.plant-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 1rem;
}

.plant-name {
    font-size: 1.2rem;
    font-weight: bold;
}

.plant-status-badge {
    padding: 0.25rem 0.75rem;
    border-radius: 20px;
    font-size: 0.8rem;
    font-weight: bold;
}

.plant-status-badge.online {
    background: rgba(34, 197, 94, 0.2);
    color: #22c55e;
    border: 1px solid rgba(34, 197, 94, 0.5);
}

.plant-status-badge.offline {
    background: rgba(239, 68, 68, 0.2);
    color: #ef4444;
    border: 1px solid rgba(239, 68, 68, 0.5);
}

.plant-info {
    margin-bottom: 1rem;
}

.plant-type {
    font-style: italic;
    color: rgba(255, 255, 255, 0.7);
    margin-bottom: 0.5rem;
}

.plant-location {
    color: rgba(255, 255, 255, 0.6);
    font-size: 0.9rem;
}

.plant-metrics {
    display: flex;
    justify-content: space-between;
    font-size: 0.9rem;
}

.plant-metric {
    text-align: center;
}

.plant-metric-value {
    font-weight: bold;
    color: #22c55e;
}

.plant-metric-label {
    color: rgba(255, 255, 255, 0.6);
    font-size: 0.8rem;
}

/* Analytics Content */
.analytics-content {
    display: grid;
    gap: 2rem;
}

.analytics-summary {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
    gap: 1rem;
    margin-bottom: 2rem;
}

.summary-card {
    background: rgba(0, 0, 0, 0.2);
    backdrop-filter: blur(10px);
    border-radius: 15px;
    padding: 1.5rem;
    border: 1px solid rgba(255, 255, 255, 0.1);
    text-align: center;
}

.summary-value {
    font-size: 2rem;
    font-weight: bold;
    color: #22c55e;
    margin-bottom: 0.5rem;
}

.summary-label {
    color: rgba(255, 255, 255, 0.8);
    font-size: 0.9rem;
}

/* Settings Content */
.settings-content {
    max-width: 600px;
    margin: 0 auto;
}

.setting-group {
    background: rgba(0, 0, 0, 0.2);
    backdrop-filter: blur(10px);
    border-radius: 15px;
    padding: 1.5rem;
    border: 1px solid rgba(255, 255, 255, 0.1);
    margin-bottom: 1.5rem;
}

.setting-group h3 {
    margin-bottom: 1rem;
    color: #22c55e;
}

.setting-group select {
    background: rgba(0, 0, 0, 0.3);
    border: 1px solid rgba(255, 255, 255, 0.2);
    border-radius: 8px;
    padding: 0.5rem;
    color: white;
    width: 100%;
    max-width: 200px;
}

.setting-group select option {
    background: #1a1a1a;
    color: white;
}

/* Switch Toggle */
.switch {
    position: relative;
    display: inline-block;
    width: 60px;
    height: 34px;
    margin-right: 1rem;
}

.switch input {
    opacity: 0;
    width: 0;
    height: 0;
}

.slider {
    position: absolute;
    cursor: pointer;
    top: 0;
    left: 0;
    right: 0;
    bottom: 0;
    background-color: #ccc;
    transition: .4s;
    border-radius: 34px;
}

.slider:before {
    position: absolute;
    content: "";
    height: 26px;
    width: 26px;
    left: 4px;
    bottom: 4px;
    background-color: white;
    transition: .4s;
    border-radius: 50%;
}

input:checked + .slider {
    background-color: #22c55e;
}

input:checked + .slider:before {
    transform: translateX(26px);
}

/* Loading States */
.loading {
    display: flex;
    justify-content: center;
    align-items: center;
    padding: 2rem;
    font-size: 1.1rem;
    color: rgba(255, 255, 255, 0.7);
}

.loading::after {
    content: '';
    width: 20px;
    height: 20px;
    border: 2px solid rgba(255, 255, 255, 0.3);
    border-top: 2px solid #22c55e;
    border-radius: 50%;
    animation: spin 1s linear infinite;
    margin-left: 1rem;
}

@keyframes spin {
    0% { transform: rotate(0deg); }
    100% { transform: rotate(360deg); }
}

/* Responsive Design */
@media (max-width: 768px) {
    .container {
        padding: 0 10px;
    }
    
    .header-content {
        flex-direction: column;
        align-items: stretch;
    }
    
    .nav {
        justify-content: center;
    }
    
    .status-header {
        flex-direction: column;
        text-align: center;
    }
    
    .frequency-display {
        min-width: auto;
        width: 100%;
    }
    
    .metrics-grid {
        grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
        gap: 1rem;
    }
    
    .charts-grid {
        grid-template-columns: 1fr;
        gap: 1rem;
    }
    
    .bands-grid {
        grid-template-columns: 1fr;
    }
    
    .status-info h2 {
        font-size: 1.5rem;
    }
    
    .frequency-value {
        font-size: 1.5rem;
    }
    
    .metric-value {
        font-size: 1.5rem;
    }
}

@media (max-width: 480px) {
    .nav {
        flex-direction: column;
    }
    
    .nav-btn {
        justify-content: center;
    }
    
    .metrics-grid {
        grid-template-columns: 1fr;
    }
    
    .chart-container {
        height: 250px;
    }
}
    </style>
</head>
<body>
    <div class="container">
        <!-- Header -->
        <header class="header">
            <div class="header-content">
                <div class="logo">
                    <span class="logo-icon"></span>
                    <h1>Plantas que Falam</h1>
                </div>
                <nav class="nav">
                    <button class="nav-btn active" data-tab="dashboard">
                        Dashboard
                    </button>
                    <button class="nav-btn" data-tab="plants">
                        Plantas
                    </button>
                    <button class="nav-btn" data-tab="analytics">
                        Analises
                    </button>
                    <button class="nav-btn" data-tab="settings">
                        Configurações
                    </button>
                </nav>
                <div class="status-indicator">
                    <span class="status-dot" id="status-dot"></span>
                    <span id="connection-status">Conectando...</span>
                </div>
            </div>
        </header>

        <!-- Main Content -->
        <main class="main-content">
            <!-- Dashboard Tab -->
            <div id="dashboard-tab" class="tab-content active">
                <!-- Status Header -->
                <div class="status-header">
                    <div class="status-info">
                        <h2>Dashboard das Plantas</h2>
                        <p>Garanta a segurança das suas plantas através de sensores piezoelétricos</p>
                    </div>
                    <div class="frequency-display">
                        <div class="frequency-value" id="dominant-freq">0.0 Hz</div>
                        <div class="frequency-label">Frequência da Comunicação</div>
                        <div class="plant-status" id="plant-status">Silêncio</div>
                    </div>
                </div>

                <!-- Metrics Grid -->
                <div class="metrics-grid">
                    <div class="metric-card">
                        <div class="metric-header">
                            <span class="metric-title">Sinal da Planta</span>
                        </div>
                        <div class="metric-value" id="raw-value">0</div>
                        <div class="metric-subtitle">Intensidade do sinal</div>
                    </div>

                    <div class="metric-card">
                        <div class="metric-header">
                            <span class="metric-title">Bioeletricidade</span>
                        </div>
                        <div class="metric-value" id="voltage">0.00 V</div>
                        <div class="metric-subtitle">Atividade elétrica</div>
                    </div>

                    <div class="metric-card">
                        <div class="metric-header">
                            <span class="metric-title">Intensidade</span>
                        </div>
                        <div class="metric-value" id="dominant-magnitude">0.000</div>
                        <div class="metric-subtitle" id="dominant-magnitude-db">0.0 dB</div>
                    </div>

                    <div class="metric-card">
                        <div class="metric-header">
                            <span class="metric-title">Atividade Média</span>
                        </div>
                        <div class="metric-value" id="average-magnitude">0.000</div>
                        <div class="metric-subtitle">Comunicação contínua</div>
                    </div>
                </div>

                <!-- Charts Grid -->
                <div class="charts-grid">
                    <div class="chart-card">
                        <div class="chart-header">
                            <h3>Histórico de Comunicação</h3>
                            <p>Últimas 20 "palavras" da sua planta</p>
                        </div>
                        <div class="chart-container">
                            <canvas id="magnitudeChart"></canvas>
                        </div>
                    </div>

                    <div class="chart-card">
                        <div class="chart-header">
                            <h3>Espectro da Voz Vegetal</h3>
                            <p>Análise das frequências de comunicação</p>
                        </div>
                        <div class="chart-container">
                            <canvas id="bandsChart"></canvas>
                        </div>
                    </div>
                </div>

                <!-- Frequency Bands List -->
                <div class="bands-card">
                    <div class="bands-header">
                        <h3>Análise Detalhada da Comunicação Vegetal</h3>
                        <p>Decodificação das diferentes "tonalidades" da sua planta</p>
                    </div>
                    <div class="bands-grid" id="bands-list">
                        <!-- Bandas serão preenchidas via JavaScript -->
                    </div>
                </div>
            </div>

            <!-- Plants Tab -->
            <div id="plants-tab" class="tab-content">
                <div class="tab-header">
                    <h2>Minhas Plantas</h2>
                    <p>Gerencie suas plantas conectadas</p>
                </div>
                <div class="plants-grid" id="plants-grid">
                    <!-- Plantas serão carregadas via JavaScript -->
                </div>
            </div>

            <!-- Analytics Tab -->
            <div id="analytics-tab" class="tab-content">
                <div class="tab-header">
                    <h2>Analytics Avançado</h2>
                    <p>Análises profundas da comunicação vegetal</p>
                </div>
                <div class="analytics-content" id="analytics-content">
                    <!-- Analytics serão carregados via JavaScript -->
                </div>
            </div>

            <!-- Settings Tab -->
            <div id="settings-tab" class="tab-content">
                <div class="tab-header">
                    <h2>Configurações</h2>
                    <p>Personalize sua experiência</p>
                </div>
                <div class="settings-content">
                    <div class="setting-group">
                        <h3>Notificações</h3>
                        <label class="switch">
                            <input type="checkbox" id="notifications-toggle" checked>
                            <span class="slider"></span>
                        </label>
                        <span>Receber alertas quando as plantas se comunicarem</span>
                    </div>
                    <div class="setting-group">
                        <h3>Frequência de Atualização</h3>
                        <select id="refresh-rate">
                            <option value="1000">1 segundo</option>
                            <option value="2000" selected>2 segundos</option>
                            <option value="5000">5 segundos</option>
                            <option value="10000">10 segundos</option>
                        </select>
                    </div>
                    <div class="setting-group">
                        <h3>Tema</h3>
                        <select id="theme-select">
                            <option value="green" selected>Verde Natureza</option>
                            <option value="dark">Escuro</option>
                            <option value="light">Claro</option>
                        </select>
                    </div>
                </div>
            </div>
        </main>
    </div>
    <script>
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
    console.log(`Intervalo de atualização alterado para ${appState.refreshRate}ms`);
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
                <h3>Tendências de Comunicação (24h)</h3>
                <p>Atividade de comunicação ao longo do dia</p>
            </div>
            <div class="chart-container">
                <canvas id="trendsChart"></canvas>
            </div>
        </div>
        
        <div class="chart-card">
            <div class="chart-header">
                <h3>Distribuição de Frequências</h3>
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
                new Notification('Sua planta está falando!', {
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
console.log('Plantas que Falam - JavaScript carregado');
console.log('API Base URL:', CONFIG.API_BASE_URL);
console.log('Intervalo de atualização:', CONFIG.REFRESH_INTERVAL + 'ms');

    </script>
</body>
</html>
)rawliteral";

// Variáveis de controle
unsigned long lastDisplay = 0;
const unsigned long displayInterval = 200; 

// Variáveis globais para dados
double dominantFreq = 0;
double maxMagnitude = 0;
double dominantDb = -80;
int rawSensorValue = 0;
double sensorVoltage = 0;
double avgMagnitude = 0;

// Status de conexão
bool wifiConnected = false;
int webRequests = 0;

// Histórico
#define HISTORY_SIZE 20
double magnitudeHistory[HISTORY_SIZE];
int historyIndex = 0;

// Bandas de frequência
struct FrequencyBand {
  String name;
  double minFreq;
  double maxFreq;
  double magnitude;
  double magnitudeDb;
  String color;
};

FrequencyBand bands[] = {
  {"Sub Bass", 20, 60, 0, -80, "#22c55e"},
  {"Bass", 60, 250, 0, -80, "#16a34a"},
  {"Low Mid", 250, 500, 0, -80, "#15803d"},
  {"Mid", 500, 2000, 0, -80, "#166534"},
  {"High Mid", 2000, 4000, 0, -80, "#14532d"},
  {"Presence", 4000, 6000, 0, -80, "#84cc16"},
  {"Brilliance", 6000, 20000, 0, -80, "#65a30d"}
};

const int numBands = sizeof(bands) / sizeof(bands[0]);

// ---------- DECLARAÇÕES ----------
void showStartupScreen();
void setupWiFi();
void setupWebServer();
void collectSamples();
void processFFT();
void analyzeData();
void updateDisplay();
void handleRoot();
void handleAPIData();
void handleAPIPlants();
void handleAPIPlantDetails();
void handleAPIAnalyticsSummary();

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tf);
  pinMode(PIEZO_PIN, INPUT);

  for (int i = 0; i < HISTORY_SIZE; i++) magnitudeHistory[i] = 0;

  showStartupScreen();
  setupWiFi();
  setupWebServer();

  Serial.println("=== SISTEMA WEB INICIADO ===");
  Serial.printf("- Pino sensor: GPIO %d\n", PIEZO_PIN);
  Serial.printf("- Frequência amostragem: %d Hz\n", SAMPLING_FREQUENCY);
  if (wifiConnected) {
    Serial.printf("- Dashboard: http://%s\n", WiFi.localIP().toString().c_str());
  }
  Serial.println("============================");
}

// ---------- LOOP ----------
void loop() {
  server.handleClient();

  rawSensorValue = analogRead(PIEZO_PIN);
  sensorVoltage = (rawSensorValue * 3.3) / 4095.0;

  collectSamples();
  processFFT();
  analyzeData();

  if (millis() - lastDisplay > displayInterval) {
    updateDisplay();
    lastDisplay = millis();
  }

  delay(10);
}

// ---------- TELAS ----------
void showStartupScreen() {
  u8g2.clearBuffer();
  u8g2.drawStr(0, 15, "ESP32 Piezo Web");
  u8g2.drawStr(0, 30, "Iniciando...");
  u8g2.drawStr(0, 45, "GPIO 34 - 10kHz");
  u8g2.sendBuffer();
  delay(2000);
}

// ---------- WIFI ----------
void setupWiFi() {
  Serial.printf("Conectando ao WiFi: %s\n", ssid);
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\nWiFi conectado!");
  } else {
    wifiConnected = false;
    Serial.println("\nFalha na conexão WiFi!");
  }
}

// ---------- WEB SERVER ----------
void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/api/sensor/data", handleAPIData);
  server.on("/api/sensor/plants", handleAPIPlants);
  server.on("/api/sensor/analytics/summary", handleAPIAnalyticsSummary);

  // Para /api/sensor/plants/{id}
  server.onNotFound([]() {
    String path = server.uri();
    if (path.startsWith("/api/sensor/plants/")) {
      handleAPIPlantDetails();
    } else {
      server.send(404, "text/plain", "Not Found");
    }
  });

  server.begin();
  Serial.println("Servidor web iniciado na porta 80");
}

// ---------- HANDLERS ----------
void handleRoot() {
  webRequests++;
  server.send(200, "text/html", HTML_CONTENT);
}

String getJSONData() {
  DynamicJsonDocument doc(2048);
  doc["timestamp"] = millis();
  doc["raw_value"] = rawSensorValue;
  doc["voltage"] = sensorVoltage;
  doc["dominant_frequency"] = dominantFreq;
  doc["dominant_magnitude"] = maxMagnitude;
  doc["dominant_magnitude_db"] = dominantDb;
  doc["average_magnitude"] = avgMagnitude;
  return doc.as<String>();
}

void handleAPIData() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", getJSONData());
}

void handleAPIPlants() {
  DynamicJsonDocument doc(1024);
  JsonArray plantsArray = doc.createNestedArray("plants");

  JsonObject plant1 = plantsArray.createNestedObject();
  plant1["id"] = 1;
  plant1["name"] = "Samambaia Falante";
  plant1["status"] = "online";

  JsonObject plant2 = plantsArray.createNestedObject();
  plant2["id"] = 2;
  plant2["name"] = "Violeta Sussurrante";
  plant2["status"] = "offline";

  String jsonString;
  serializeJson(doc, jsonString);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonString);
}

void handleAPIPlantDetails() {
  String path = server.uri();
  int lastSlash = path.lastIndexOf('/');
  int plantId = path.substring(lastSlash + 1).toInt();

  DynamicJsonDocument doc(512);
  doc["plant_id"] = plantId;
  doc["raw_value"] = rawSensorValue;
  doc["dominant_frequency"] = dominantFreq;
  doc["status"] = (maxMagnitude > 0.05) ? "online" : "offline";

  String jsonString;
  serializeJson(doc, jsonString);
  server.send(200, "application/json", jsonString);
}

void handleAPIAnalyticsSummary() {
  DynamicJsonDocument doc(512);
  doc["total_plants"] = 2;
  doc["active_plants"] = 1;
  doc["average_frequency"] = 450.0;

  String jsonString;
  serializeJson(doc, jsonString);
  server.send(200, "application/json", jsonString);
}

// ---------- PROCESSAMENTO ----------
void collectSamples() {
  unsigned long samplingPeriod = 1000000L / SAMPLING_FREQUENCY;
  for (int i = 0; i < SAMPLES; i++) {
    unsigned long startTime = micros();
    int sensorValue = analogRead(PIEZO_PIN);
    double voltage = (sensorValue * 3.3) / 4095.0;
    vReal[i] = voltage - 1.65;
    vImag[i] = 0.0;
    while (micros() - startTime < samplingPeriod);
  }
}

void processFFT() {
  FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();
}

void analyzeData() {
  double frequencyResolution = (double)SAMPLING_FREQUENCY / SAMPLES;
  maxMagnitude = 0; dominantFreq = 0;

  for (int i = 1; i < (SAMPLES / 2); i++) {
    double frequency = i * frequencyResolution;
    double magnitude = vReal[i];
    if (magnitude > maxMagnitude) {
      maxMagnitude = magnitude;
      dominantFreq = frequency;
    }
  }
  dominantDb = 20 * log10(maxMagnitude + 0.001);
}

void updateDisplay() {
  u8g2.clearBuffer();
  char line[32];
  sprintf(line, "RAW:%d", rawSensorValue);
  u8g2.drawStr(0, 10, line);
  sprintf(line, "FREQ:%.1fHz", dominantFreq);
  u8g2.drawStr(0, 25, line);
  sprintf(line, "MAG:%.3f", maxMagnitude);
  u8g2.drawStr(0, 40, line);
  sprintf(line, "DB:%.1f", dominantDb);
  u8g2.drawStr(0, 55, line);
  u8g2.sendBuffer();
}
