// Backend address. Local (start backend with uvicorn on port 8100) or deployed (Render).
window.API_URL = ["localhost", "127.0.0.1"].includes(location.hostname)
  ? "http://127.0.0.1:8100"
  : "https://xiao-http-api.onrender.com";
