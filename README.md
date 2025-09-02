# 🌱 Plantas que Falam

Um sistema de monitoramento inteligente que utiliza **sensores piezoelétricos** conectados ao **ESP32** para captar sinais bioelétricos de plantas, interpretar sua comunicação e exibir os dados em um **painel web interativo**.  
O projeto combina **IoT, Flask, JavaScript e Chart.js**, permitindo análise em tempo real da atividade elétrica vegetal.

---

## 🚀 Funcionalidades

- 📡 **Coleta de dados** em tempo real a partir do ESP32.  
- 📊 **Dashboard interativo** com gráficos de comunicação das plantas (histórico e espectro).  
- 🌍 **Gerenciamento de múltiplas plantas** com status online/offline.  
- 🔔 **Notificações do navegador** quando a planta “fala”.  
- 🎨 **Personalização de tema** (Verde Natureza, Escuro e Claro).  
- ⚙️ **Configurações dinâmicas** de taxa de atualização e preferências.  
- 📈 **Módulo de Analytics** com resumo de tendências e distribuições de frequência.

---

## ⚙️ Tecnologias Utilizadas

- **Hardware:** ESP32 + sensor piezoelétrico  
- **Frontend:** HTML5, CSS3, JavaScript (Chart.js)  
- **Backend:** Python (Flask + Flask-CORS + SQLAlchemy)  
- **Banco de Dados:** SQLite (opcional, já configurado)  
- **Comunicação:** API REST `/api/sensor`

---

## 📊 Demonstração do Dashboard

- **Dashboard:** Mostra frequência dominante, intensidade e atividade média.

- **Plantas**: Lista e status de todas as plantas conectadas.

- **Analises**: Tendências das últimas 24h e distribuição de frequências.

- **Configurações:** Preferências de tema, taxa de atualização e notificações.

---

## 👨‍💻 Autor

- Projeto desenvolvido por **Bernardo Locatelli Araújo**.
- Estudante de **Desenvolvimento de Sistemas** no SENAI São Mateus.
