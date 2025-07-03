from flask import Flask, jsonify
import requests
from datetime import datetime, timedelta

app = Flask(__name__)

API_KEY = "2a058a572f264c8ba2b124925253006"

def get_avg_rainfall():
    res = requests.get("http://ip-api.com/json").json()
    lat = res["lat"]
    lon = res["lon"]

    total_rainfall = 0.0
    days_counted = 0

    for i in range(10):
        date = (datetime.now() - timedelta(days=i)).strftime('%Y-%m-%d')
        url = f"http://api.weatherapi.com/v1/history.json?key={API_KEY}&q={lat},{lon}&dt={date}"
        response = requests.get(url)
        data = response.json()

        try:
            rainfall = data["forecast"]["forecastday"][0]["day"]["totalprecip_mm"]
            total_rainfall += rainfall
            days_counted += 1
        except KeyError:
            continue

    avg_rainfall = total_rainfall / days_counted if days_counted > 0 else 0.0
    return avg_rainfall

@app.route('/rainfall', methods=['GET'])
def rainfall():
    avg_rainfall = get_avg_rainfall()
    return jsonify({"average_rainfall": avg_rainfall})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
