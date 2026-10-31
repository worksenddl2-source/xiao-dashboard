-- Supabase setup for HttpDashboard (FastAPI backend + XIAO over HTTP)
-- Run once in Supabase Dashboard > SQL Editor.
-- - Logged-in users (Supabase Auth) can read the device and change the LED.
-- - The board never talks to Supabase directly: the backend calls
--   api_device_report() with the board's device key.

create extension if not exists pgcrypto with schema extensions;

-- 1. Device state (one row per board)
create table public.api_device (
  id text primary key,
  led boolean not null default false,
  touched boolean not null default false,
  touch_value integer,
  baseline integer,
  rssi integer,
  seen_at timestamptz,
  led_changed_at timestamptz not null default now(),
  led_changed_by uuid references auth.users (id) on delete set null
);

insert into public.api_device (id) values ('xiao');

alter table public.api_device enable row level security;

create policy "api_device read (logged in)" on public.api_device
  for select to authenticated using (true);
create policy "api_device led update (logged in)" on public.api_device
  for update to authenticated using (true) with check (true);

revoke all on public.api_device from anon, authenticated;
grant select on public.api_device to authenticated;
grant update (led) on public.api_device to authenticated;

-- LED change from a user -> remember who and when
create or replace function public.api_device_led_changed()
returns trigger
language plpgsql
set search_path = ''
as $$
begin
  if new.led is distinct from old.led then
    new.led_changed_at := now();
    new.led_changed_by := auth.uid();
  end if;
  return new;
end;
$$;

create trigger api_device_led_changed
  before update on public.api_device
  for each row execute function public.api_device_led_changed();

-- 2. Device keys (only the SHA-256 hash is stored, nobody can read this table)
create table public.api_device_key (
  device_id text primary key references public.api_device (id) on delete cascade,
  key_hash text not null
);

alter table public.api_device_key enable row level security;
revoke all on public.api_device_key from anon, authenticated;

-- Register the board's key (replace with the DEVICE_KEY from secrets.h):
-- insert into public.api_device_key values
--   ('xiao', encode(extensions.digest('<DEVICE_KEY>', 'sha256'), 'hex'));

-- 3. Board report: check the key, save the sensor values, return the LED state
create or replace function public.api_device_report(
  p_device text, p_key text, p_value integer, p_baseline integer, p_touched boolean, p_rssi integer
)
returns boolean
language plpgsql
security definer
set search_path = ''
as $$
declare
  v_led boolean;
begin
  if not exists (
    select 1 from public.api_device_key k
    where k.device_id = p_device
      and k.key_hash = encode(extensions.digest(p_key, 'sha256'), 'hex')
  ) then
    raise exception 'invalid device key' using errcode = '28000';
  end if;

  update public.api_device
     set touch_value = p_value, baseline = p_baseline, touched = p_touched,
         rssi = p_rssi, seen_at = now()
   where id = p_device
  returning led into v_led;

  return v_led;
end;
$$;

revoke all on function public.api_device_report from public;
grant execute on function public.api_device_report to anon;
