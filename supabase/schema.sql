-- Supabase setup for SupabaseDashboard + web/index.html
-- Run once in Supabase Dashboard > SQL Editor.
-- Open access: anyone with the publishable key can read the row and change the LED.

-- 1. Device state table (one row per board)
create table public.xiao_device (
  id text primary key,
  led boolean not null default false,
  touched boolean not null default false,
  touch_value integer,
  baseline integer,
  rssi integer,
  seen_at timestamptz,
  led_changed_at timestamptz not null default now(),
  local_ip text,
  camera_ok boolean
);

insert into public.xiao_device (id) values ('xiao');

-- 2. Row Level Security: read and update only, no insert/delete
alter table public.xiao_device enable row level security;

create policy "xiao_device public read" on public.xiao_device
  for select to anon, authenticated using (true);
create policy "xiao_device public update" on public.xiao_device
  for update to anon, authenticated using (true) with check (true);

revoke all on public.xiao_device from anon, authenticated;
grant select on public.xiao_device to anon, authenticated;
grant update (led, touched, touch_value, baseline, rssi, seen_at, led_changed_at, local_ip, camera_ok)
  on public.xiao_device to anon, authenticated;

-- 3. Realtime: push row changes to the web dashboard
alter publication supabase_realtime add table public.xiao_device;

-- 4. Timestamps: LED change from web -> led_changed_at; board report -> seen_at
create or replace function public.xiao_device_touch_timestamps()
returns trigger
language plpgsql
set search_path = ''
as $$
begin
  if new.led is distinct from old.led then
    new.led_changed_at := now();
  else
    new.seen_at := now();
  end if;
  return new;
end;
$$;

create trigger xiao_device_timestamps
  before update on public.xiao_device
  for each row execute function public.xiao_device_touch_timestamps();
