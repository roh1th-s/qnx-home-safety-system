# QNX Home Safety Dashboard

A real-time sensor monitoring dashboard built with Preact, Recharts, and Tailwind CSS.


## Monitored Sensors

- **Temperature** - Environmental temperature monitoring
- **Humidity** - Relative humidity tracking
- **CO₂ Levels** - Air quality monitoring
- **Smoke Detector** - Fire safety alerts
- **Motion Sensor** - Intrusion detection
- **Door Sensor** - Entry/exit monitoring

## Setup

### 1. Install Dependencies

```bash
npm install
```

### 2. Configure API Endpoint

Update the API endpoint in `src/SensorDashboard.tsx`:

```typescript
const API_ENDPOINT = 'http://your-pi-ip:8000/dashboard.json';
```

Or create a `.env` file:

```
VITE_API_ENDPOINT=http://192.168.1.100:8000/dashboard.json
```

### 3. Run Development Server

```bash
npm run dev
```

### 4. Build for Production

```bash
npm run build
```

### 5. Deploy to QNX Pi

```bash
npm run deploy
```

## API Response Format

Your QNX system should serve a JSON file at the configured endpoint with the following structure:

```json
{
  "sensors": {
    "door": {
      "status": "open"
    },
    "temperature": {
      "value": 24.5
    },
    "humidity": {
      "value": 55
    },
    "smoke": {
      "status": "clear",
      "alert": false
    },
    "motion": {
      "status": "clear"
    },
    "co2": {
      "value": 450
    }
  }
}
```



