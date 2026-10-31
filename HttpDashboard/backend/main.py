"""XIAO ESP32-S3 HTTP API (FastAPI + Supabase)

Users:   POST /auth/signup, POST /auth/login, POST /auth/refresh, GET /auth/me
         GET /api/device, POST /api/led            (Authorization: Bearer <access_token>)
Board:   POST /device/report                       (X-Device-Key: <DEVICE_KEY>)

Supabase does the real work: Auth for accounts, the api_device table (RLS) for
state, and the api_device_report() function for the board. See ../schema.sql.
"""

import os

import httpx
from fastapi import FastAPI, Header, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

SUPABASE_URL = os.environ.get("SUPABASE_URL", "https://pzqdxctoadliljgipamp.supabase.co")
SUPABASE_KEY = os.environ.get("SUPABASE_KEY", "sb_publishable_hkTnsi27BQvG-ltL4MmXwQ_MwioqaoA")
DEVICE_ID = os.environ.get("DEVICE_ID", "xiao")
CORS_ORIGINS = os.environ.get("CORS_ORIGINS", "*").split(",")

app = FastAPI(title="XIAO HTTP API", version="1.0.0")
app.add_middleware(
    CORSMiddleware,
    allow_origins=CORS_ORIGINS,
    allow_methods=["*"],
    allow_headers=["*"],
)

http = httpx.AsyncClient(base_url=SUPABASE_URL, timeout=10)


class Credentials(BaseModel):
    email: str
    password: str


class RefreshBody(BaseModel):
    refresh_token: str


class LedBody(BaseModel):
    on: bool


class Report(BaseModel):
    value: int
    baseline: int
    touched: bool
    rssi: int | None = None


def supabase_headers(token: str | None = None) -> dict:
    return {
        "apikey": SUPABASE_KEY,
        "Authorization": f"Bearer {token or SUPABASE_KEY}",
    }


def bearer(authorization: str | None) -> str:
    if not authorization or not authorization.lower().startswith("bearer "):
        raise HTTPException(401, "로그인이 필요합니다 (Authorization: Bearer <token>)")
    return authorization.split(" ", 1)[1]


def check(res: httpx.Response) -> dict | list:
    """Pass Supabase errors through with a readable message."""
    if res.is_success:
        return res.json() if res.content else {}
    try:
        body = res.json()
    except ValueError:
        body = {}
    msg = body.get("msg") or body.get("message") or body.get("error_description") or res.text
    raise HTTPException(res.status_code, msg)


def session(data: dict) -> dict:
    return {
        "access_token": data["access_token"],
        "refresh_token": data["refresh_token"],
        "expires_in": data["expires_in"],
        "user": {"id": data["user"]["id"], "email": data["user"]["email"]},
    }


@app.get("/")
async def health():
    return {"ok": True, "docs": "/docs"}


# ---------- Auth ----------

@app.post("/auth/signup")
async def signup(body: Credentials):
    data = check(await http.post("/auth/v1/signup", headers=supabase_headers(), json=body.model_dump()))
    if data.get("access_token"):
        return session(data)
    # Email confirmation is on: no session until the user clicks the mail link
    return {"message": "가입 완료. 이메일 인증 후 로그인하세요.", "user": {"email": body.email}}


@app.post("/auth/login")
async def login(body: Credentials):
    res = await http.post("/auth/v1/token?grant_type=password", headers=supabase_headers(), json=body.model_dump())
    return session(check(res))


@app.post("/auth/refresh")
async def refresh(body: RefreshBody):
    res = await http.post("/auth/v1/token?grant_type=refresh_token", headers=supabase_headers(), json=body.model_dump())
    return session(check(res))


@app.get("/auth/me")
async def me(authorization: str | None = Header(None)):
    data = check(await http.get("/auth/v1/user", headers=supabase_headers(bearer(authorization))))
    return {"id": data["id"], "email": data["email"]}


# ---------- Device (logged-in users) ----------

@app.get("/api/device")
async def get_device(authorization: str | None = Header(None)):
    res = await http.get(
        "/rest/v1/api_device",
        headers=supabase_headers(bearer(authorization)),
        params={"id": f"eq.{DEVICE_ID}", "select": "*"},
    )
    rows = check(res)
    if not rows:
        raise HTTPException(404, "장치를 찾을 수 없습니다")
    return rows[0]


@app.post("/api/led")
async def set_led(body: LedBody, authorization: str | None = Header(None)):
    res = await http.patch(
        "/rest/v1/api_device",
        headers={**supabase_headers(bearer(authorization)), "Prefer": "return=representation"},
        params={"id": f"eq.{DEVICE_ID}"},
        json={"led": body.on},
    )
    rows = check(res)
    if not rows:
        raise HTTPException(404, "장치를 찾을 수 없습니다")
    return {"led": rows[0]["led"], "led_changed_at": rows[0]["led_changed_at"]}


# ---------- Board ----------

@app.post("/device/report")
async def device_report(body: Report, x_device_key: str | None = Header(None)):
    if not x_device_key:
        raise HTTPException(401, "X-Device-Key 헤더가 필요합니다")
    res = await http.post(
        "/rest/v1/rpc/api_device_report",
        headers=supabase_headers(),
        json={
            "p_device": DEVICE_ID,
            "p_key": x_device_key,
            "p_value": body.value,
            "p_baseline": body.baseline,
            "p_touched": body.touched,
            "p_rssi": body.rssi,
        },
    )
    if res.status_code in (400, 401, 403) and "invalid device key" in res.text:
        raise HTTPException(403, "잘못된 장치 키")
    return {"led": check(res)}
