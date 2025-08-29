from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse
import paho.mqtt.client as mqtt
from langchain.agents import initialize_agent, AgentType, Tool
from langchain_google_genai import ChatGoogleGenerativeAI
import uvicorn
import json

# --- MQTT Setup ---
BROKER = "broker.hivemq.com"
PORT = 1883
TOPIC_LED = "esp32/led/control"
TOPIC_SERVO = "esp32/door/control"
TOPIC_SENSORS = "esp32/sensors/data"

mqtt_client = mqtt.Client()
latest_sensors = None
led_state = "OFF"

def on_connect(client, userdata, flags, rc):
    print("Connected with result code", rc)
    client.subscribe(TOPIC_SENSORS)

def on_message(client, userdata, msg):
    global latest_sensors
    if msg.topic == TOPIC_SENSORS:
        try:
            # Parse JSON
            latest_sensors = json.loads(msg.payload.decode())
            print("Received sensors:", latest_sensors)

            # Extract values with the correct keys
            temp  = latest_sensors.get("T")
            humid = latest_sensors.get("H")
            ldr   = latest_sensors.get("LDR")
            led   = latest_sensors.get("LED")
            door  = latest_sensors.get("DOOR")
            gas   = latest_sensors.get("Gas")

            print(f"T={temp}, H={humid}, LDR={ldr}, LED={led}, DOOR={door}, GAS={gas}")

        except json.JSONDecodeError:
            latest_sensors = None

mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message
mqtt_client.connect(BROKER, PORT, 60)
mqtt_client.loop_start()

# --- Tool Functions ---
def get_sensors_reading(_=None):
    return str(latest_sensors) if latest_sensors is not None else "Sensors reading not available"

def turn_led_on(_=None):
    global led_state
    if led_state == "ON":
        return "LED is already ON"
    mqtt_client.publish(TOPIC_LED, "ON")
    led_state = "ON"
    return "LED turned ON"

def turn_led_off(_=None):
    global led_state
    if led_state == "OFF":
        return "LED is already OFF"
    mqtt_client.publish(TOPIC_LED, "OFF")
    led_state = "OFF"
    return "LED turned OFF"

def move_servo(command: str):  # may be edit
    cmd = command.strip().upper()
    if cmd not in ["OPEN", "CLOSE"]:
        return "Invalid command. Use 'OPEN' or 'CLOSE'."
    
    mqtt_client.publish(TOPIC_SERVO, cmd)
    return f"Door command sent: {cmd}"



# --- LangChain Agent ---
tools = [
    Tool(name="Get Sensors Reading", func=get_sensors_reading, description="Get the latest sensor readings (Temp, Humidity, LDR, Gas, LED, Door)."),
    Tool(name="Turn LED ON", func=turn_led_on, description="Turn the LED ON."),
    Tool(name="Turn LED OFF", func=turn_led_off, description="Turn the LED OFF."),
    Tool(name="Move Servo", func=move_servo, description="Move servo to a specified Position ."),
]

system_prompt = """
You are an IoT home assistant...

Available tools:
- Get Sensors Reading → returns latest Sensors values.
- Turn LED ON → turns the LED ON.
- Turn LED OFF → turns the LED OFF.
- Move Servo → moves the servo motor to a specified Postion (OPEN-CLOSED).

Rules:
1. ALWAYS use the tools — do not explain, just call the tool.
2. For Sensors : if none, respond exactly “ Sensors reading not available”.
3. For LED: report if already in requested state.
4. Servo: input must be OPEN-CLOSED.
5. If impossible, reply with: “I cannot do that.”
"""

llm = ChatGoogleGenerativeAI(
    model="gemini-2.5-flash",
    google_api_key="AIzaSyAz-dgyGIPo90kpezTjraeMd71GzK_YLao"
)
 
agent = initialize_agent(
    tools=tools,
    llm=llm,
    agent=AgentType.STRUCTURED_CHAT_ZERO_SHOT_REACT_DESCRIPTION,
    verbose=True,
    handle_parsing_errors=True,
    return_only_outputs=True
)

# --- FastAPI App ---
app = FastAPI()

@app.post("/chat")
async def chat(request: Request):
    data = await request.json()
    user_input = data.get("text", "")
    try:
        reply = agent.invoke({
            "input": user_input,
            "chat_history": [{"role": "system", "content": system_prompt}]
        })
        return JSONResponse({"reply": reply.get("output", "")})
    except Exception as e:
        return JSONResponse({"error": str(e)}, status_code=500)

if _name_ == "_main_":
    uvicorn.run("bot:app", host="0.0.0.0", port=8884, reload=True)