// Configuration file for IoT Smart Environmental Monitoring System

// --- MQTT Configuration ---
// Used for real-time communication between the ESP32 device and the web dashboard.
const MQTT_CONFIG = {
    brokerUrl: 'wss://broker.hivemq.com:8884/mqtt', // For web client (secure websocket)
    mqttServer: 'broker.hivemq.com', // For ESP32 client
    mqttPort: 1883, // Standard unencrypted MQTT port for ESP32
    options: {
        clientId: 'web-client-' + Math.random().toString(16).substr(2, 8),
        username: '', // Leave empty for HiveMQ public broker
        password: '', // Leave empty for HiveMQ public broker
        clean: true,
        reconnectPeriod: 1000,
        connectTimeout: 30 * 1000
    },
    topics: {
        sensorData: 'esp32/sensors/data',   // Topic for publishing sensor readings
        ledControl: 'esp32/led/control',     // Topic for controlling the LED
        doorControl: 'esp32/door/control',   // Topic for controlling a door/servo
        ledStatus: 'esp32/led/status',       // Topic for getting LED status feedback
        doorStatus: 'esp32/door/status'      // Topic for getting door status feedback
    }
};

// --- Supabase Configuration ---
// Used for storing historical sensor data in a cloud database.
const SUPABASE_CONFIG = {
    url: 'https://qroeztvhjoydenryhxfl.supabase.co',
    // !!! SECURITY WARNING !!!
    // The key below seems to be a service_role or secret key.
    // DO NOT expose this key in client-side code (like a public website).
    // It gives full admin access to your database. Use the 'anon' key instead for public access.
    key: 'sb_secret_is_dY-YDUEZK-1x2Aj3cGg_GsPM3U-p',
    tables: {
        dht22: 'dht22',
        gasSensor: 'gas-sensor',
        led: 'led',
        photoresistor: 'photoresistor-sensor'
    }
};

// --- Application Settings ---
// General settings for the web dashboard or application logic.
const APP_CONFIG = {
    refreshInterval: 5000, // Data refresh interval in milliseconds (e.g., for charts)
    gasAlertThreshold: 2000, // PPM value to trigger a gas leak alert
    temperatureUnits: 'celsius', // Supported values: 'celsius' or 'fahrenheit'
};

// --- User Authentication (For Demo Purposes Only) ---
// !!! SECURITY WARNING !!!
// Storing passwords in plaintext is extremely insecure.
// This is for demonstration/testing ONLY. Use a proper authentication service
// with hashed passwords for any real application.
const USERS = [
    { email: 'user1@user.com', password: 'user1user1user1' },
    { email: 'user2@user.com', password: 'user2user2user2' },
    { email: 'user3@user.com', password: 'user3user3user3' }
];

// --- Export Configurations ---
// Makes these settings available to other files in a Node.js project.
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        MQTT_CONFIG,
        SUPABASE_CONFIG,
        APP_CONFIG,
        USERS
    };
}