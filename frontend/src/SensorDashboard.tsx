/**
 * Sensor Dashboard - Real-time Environmental Monitoring
 *
 * Configuration:
 * 1. Update the API_ENDPOINT below with your QNX Pi's IP address
 * 2. Or create a .env file with: VITE_API_ENDPOINT=http://your-pi-ip:8000/dashboard.json
 * 3. Ensure your Pi is serving dashboard.json on port 8000
 *
 * Expected API Response Format:
 * {
 *   "sensors": {
 *     "door": { "status": "open" | "closed" },
 *     "temperature": { "value": number },
 *     "humidity": { "value": number },
 *     "smoke": { "status": string, "alert": boolean },
 *     "motion": { "status": "detected" | "clear" },
 *     "co2": { "value": number }
 *   }
 * }
 */

import { useState, useEffect } from "preact/compat";
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  ResponsiveContainer,
} from "recharts";
import { Thermometer, Droplets, Wind, Bell, AlertTriangle } from "lucide-react";

interface DataPoint {
  time: string;
  value: number;
}

interface Alert {
  id: number;
  type: string;
  message: string;
  severity: "warning" | "critical";
  timestamp: string;
}

interface SensorData {
  temperature: DataPoint[];
  humidity: DataPoint[];
  co2: DataPoint[];
}

interface ApiResponse {
  sensors: {
    door: {
      status: string;
    };
    temperature: {
      value: number;
      alert: boolean;
    };
    humidity?: {
      value: number;
    };
    motion: {
      status: string;
    };
    co2?: {
      value: number;
      alert: boolean;
    };
  };
}

const API_ENDPOINT = import.meta.env.VITE_API_ENDPOINT;

const SensorDashboard = () => {
  const [sensorData, setSensorData] = useState<SensorData>({
    temperature: [],
    humidity: [],
    co2: [],
  });
  const [alerts, setAlerts] = useState<Alert[]>([]);
  const [currentValues, setCurrentValues] = useState({
    temperature: 22,
    humidity: 45,
    co2: 420,
  });
  const [connectionStatus, setConnectionStatus] = useState<"online" | "offline" | "error">(
    "offline"
  );

  // Fetch real-time sensor data from API
  useEffect(() => {
    const fetchData = () => {
      fetch(API_ENDPOINT)
        .then((response) => {
          if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
          }
          return response.json();
        })
        .then((data: ApiResponse) => {
          const timestamp = new Date().toLocaleTimeString();
          const sensors = data.sensors;

          // Extract sensor values
          const temp = sensors.temperature?.value || 22;
          const humid = sensors.humidity?.value || 45;
          const co2Level = sensors.co2?.value || 420;

          setCurrentValues({
            temperature: temp,
            humidity: humid,
            co2: co2Level,
          });

          // Update chart data
          setSensorData((prev) => ({
            temperature: [...prev.temperature, { time: timestamp, value: temp }].slice(-20),
            humidity: [...prev.humidity, { time: timestamp, value: humid }].slice(-20),
            co2: [...prev.co2, { time: timestamp, value: co2Level }].slice(-20),
          }));

          // Check for alerts based on sensor data
          const newAlerts: Array<Omit<Alert, "id" | "timestamp">> = [];

          // Temperature alert
          if (sensors.temperature?.alert) {
            newAlerts.push({
              type: "temperature",
              message: "🌡️ Temperature alert!",
              severity: "warning" as const,
            });
          }

          // CO2 alert
          if (sensors.co2?.alert) {
            newAlerts.push({
              type: "co2",
              message: "💨 CO₂ alert!",
              severity: "warning" as const,
            });
          }

          // Motion alert
          if (sensors.motion?.status === "detected") {
            newAlerts.push({
              type: "motion",
              message: "🚶 Motion detected in monitored area!",
              severity: "warning" as const,
            });
          }

          // Door alert
          if (sensors.door?.status === "open") {
            newAlerts.push({
              type: "door",
              message: "🚪 Door is open!",
              severity: "warning" as const,
            });
          }

          // Add new alerts
          newAlerts.forEach((alert) => addAlert(alert));

          setConnectionStatus("online");
        })
        .catch((error) => {
          console.error("Error fetching sensor data:", error);
          setConnectionStatus("error");
        });
    };

    // Fetch immediately on mount
    fetchData();

    // Then fetch every 2 seconds
    const interval = setInterval(fetchData, 2000);

    return () => clearInterval(interval);
  }, []);

  const addAlert = (alert: Omit<Alert, "id" | "timestamp">) => {
    setAlerts((prev) => {
      // Check if this alert type already exists in the current alerts
      const alreadyExists = prev.some((a) => a.type === alert.type);
      if (alreadyExists) {
        return prev; // Don't add duplicate
      }

      const newAlert: Alert = {
        id: Date.now(),
        ...alert,
        timestamp: new Date().toLocaleTimeString(),
      };

      // Auto-remove alert after 5 seconds
      setTimeout(() => {
        setAlerts((current) => current.filter((a) => a.id !== newAlert.id));
      }, 5000);

      return [newAlert, ...prev].slice(0, 5);
    });
  };

  const StatCard = ({
    icon: Icon,
    title,
    value,
    unit,
    color,
    status,
  }: {
    icon: any;
    title: string;
    value: number;
    unit: string;
    color: string;
    status: string;
  }) => (
    <div
      className="bg-white rounded-lg shadow-lg p-6 border-l-4"
      style={{ borderLeftColor: color }}
    >
      <div className="flex items-center justify-between">
        <div>
          <p className="text-gray-500 text-sm font-medium">{title}</p>
          <p className="text-3xl font-bold mt-2" style={{ color }}>
            {value.toFixed(1)}
            <span className="text-xl ml-1">{unit}</span>
          </p>
          <p className="text-xs text-gray-400 mt-1">{status}</p>
        </div>
        <div className="p-3 rounded-full" style={{ backgroundColor: `${color}20` }}>
          <Icon size={32} color={color} />
        </div>
      </div>
    </div>
  );

  const AlertToast = ({ alert }: { alert: Alert }) => (
    <div
      className={`w-full md:w-auto flex items-start gap-3 p-4 rounded-lg shadow-lg border-l-4 mb-3 animate-slide-in ${
        alert.severity === "critical" ? "bg-red-50 border-red-500" : "bg-amber-50 border-amber-500"
      }`}
    >
      <AlertTriangle
        size={24}
        className={alert.severity === "critical" ? "text-red-500" : "text-amber-500"}
      />
      <div className="flex-1">
        <p className="font-semibold text-gray-900">{alert.message}</p>
        <p className="text-xs text-gray-500 mt-1">{alert.timestamp}</p>
      </div>
    </div>
  );

  const getStatus = (type: string, value: number) => {
    switch (type) {
      case "temperature":
        if (value < 18) return "Too Cold";
        if (value > 27) return "Too Hot";
        return "Optimal";
      case "humidity":
        if (value < 30) return "Too Dry";
        if (value > 60) return "Too Humid";
        return "Comfortable";
      case "co2":
        if (value > 700) return "Poor Air Quality";
        if (value > 500) return "Fair";
        return "Good Air Quality";
      default:
        return "Normal";
    }
  };

  return (
    <div className="min-h-screen bg-linear-to-br from-blue-50 via-indigo-50 to-purple-50 p-6">
      <style>{`
        @keyframes slide-in {
          from {
            transform: translateX(400px);
            opacity: 0;
          }
          to {
            transform: translateX(0);
            opacity: 1;
          }
        }
        .animate-slide-in {
          animation: slide-in 0.3s ease-out;
        }
      `}</style>

      {/* Header */}
      <div className="mb-8">
        <h1 className="text-4xl font-bold text-gray-800 mb-2">Sensor Monitoring Dashboard</h1>
        <p className="text-gray-600">Real-time environmental data tracking</p>
      </div>

      {/* Alert Toasts - Fixed position */}
      <div className="fixed top-6 right-6 z-50 w-96 max-w-[90%]">
        {alerts.map((alert) => (
          <AlertToast key={alert.id} alert={alert} />
        ))}
      </div>

      {/* Stats Cards */}
      <div className="grid grid-cols-1 md:grid-cols-3 gap-6 mb-8">
        <StatCard
          icon={Thermometer}
          title="Temperature"
          value={currentValues.temperature}
          unit="°C"
          color="#ef4444"
          status={getStatus("temperature", currentValues.temperature)}
        />
        <StatCard
          icon={Droplets}
          title="Humidity"
          value={currentValues.humidity}
          unit="%"
          color="#3b82f6"
          status={getStatus("humidity", currentValues.humidity)}
        />
        <StatCard
          icon={Wind}
          title="CO₂ Level"
          value={currentValues.co2}
          unit="ppm"
          color="#10b981"
          status={getStatus("co2", currentValues.co2)}
        />
      </div>

      {/* Charts */}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        {/* Temperature Chart */}
        <div className="bg-white rounded-lg shadow-lg p-6">
          <h3 className="text-lg font-semibold text-gray-800 mb-4 flex items-center gap-2">
            <Thermometer size={20} className="text-red-500" />
            Temperature Trend
          </h3>
          <ResponsiveContainer width="100%" height={250}>
            <LineChart data={sensorData.temperature}>
              <CartesianGrid strokeDasharray="3 3" stroke="#f0f0f0" />
              <XAxis dataKey="time" tick={{ fontSize: 12 }} />
              <YAxis domain={[15, 35]} tick={{ fontSize: 12 }} />
              <Tooltip />
              <Line
                type="monotone"
                dataKey="value"
                stroke="#ef4444"
                strokeWidth={2}
                dot={false}
                name="Temperature (°C)"
              />
            </LineChart>
          </ResponsiveContainer>
        </div>

        {/* Humidity Chart */}
        <div className="bg-white rounded-lg shadow-lg p-6">
          <h3 className="text-lg font-semibold text-gray-800 mb-4 flex items-center gap-2">
            <Droplets size={20} className="text-blue-500" />
            Humidity Trend
          </h3>
          <ResponsiveContainer width="100%" height={250}>
            <LineChart data={sensorData.humidity}>
              <CartesianGrid strokeDasharray="3 3" stroke="#f0f0f0" />
              <XAxis dataKey="time" tick={{ fontSize: 12 }} />
              <YAxis domain={[0, 100]} tick={{ fontSize: 12 }} />
              <Tooltip />
              <Line
                type="monotone"
                dataKey="value"
                stroke="#3b82f6"
                strokeWidth={2}
                dot={false}
                name="Humidity (%)"
              />
            </LineChart>
          </ResponsiveContainer>
        </div>

        {/* CO2 Chart */}
        <div className="bg-white rounded-lg shadow-lg p-6 lg:col-span-2">
          <h3 className="text-lg font-semibold text-gray-800 mb-4 flex items-center gap-2">
            <Wind size={20} className="text-green-500" />
            CO₂ Level Trend
          </h3>
          <ResponsiveContainer width="100%" height={250}>
            <LineChart data={sensorData.co2}>
              <CartesianGrid strokeDasharray="3 3" stroke="#f0f0f0" />
              <XAxis dataKey="time" tick={{ fontSize: 12 }} />
              <YAxis domain={[300, 1000]} tick={{ fontSize: 12 }} />
              <Tooltip />
              <Line
                type="monotone"
                dataKey="value"
                stroke="#10b981"
                strokeWidth={2}
                dot={false}
                name="CO₂ (ppm)"
              />
            </LineChart>
          </ResponsiveContainer>
        </div>
      </div>

      {/* Status Indicator */}
      <div className="mt-6 bg-white rounded-lg shadow-lg p-4 flex items-center justify-between">
        <div className="flex items-center gap-2">
          <div
            className={`w-3 h-3 rounded-full ${
              connectionStatus === "online"
                ? "bg-green-500 animate-pulse"
                : connectionStatus === "error"
                ? "bg-red-500"
                : "bg-yellow-500"
            }`}
          ></div>
          <span className="text-sm font-medium text-gray-700">
            {connectionStatus === "online"
              ? "System Online - Data Updating Every 2 Seconds"
              : connectionStatus === "error"
              ? "Connection Error - Retrying..."
              : "Connecting to Sensors..."}
          </span>
        </div>
        <Bell size={20} className="text-gray-400" />
      </div>
    </div>
  );
};

export default SensorDashboard;
