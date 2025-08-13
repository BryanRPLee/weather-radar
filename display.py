import argparse
import math
import threading
import time
import warnings

import matplotlib
import matplotlib.pyplot as plt
import matplotlib.colors as mcolors
import matplotlib.cm as cm
import matplotlib.patches as mpatches
import numpy as np
import requests

matplotlib.use('TkAgg')

POLL_HZ       = 20
ESP32_URL     = "http://192.168.4.1/radardata"
MAX_RANGE_NM  = 100.0
DBZ_DECAY     = 0.995
FETCH_TIMEOUT = 0.3

DBZ_BOUNDS = [0, 20, 25, 30, 35, 40, 45, 50, 55, 60, 75]
DBZ_COLORS = [
    '#000a00',
    '#00ecec',
    '#00a000',
    '#00d000',
    '#ffff00',
    '#e7c000',
    '#ff9000',
    '#ff0000',
    '#d40000',
    '#ff00ff',
    '#9955ff',
]
DBZ_CMAP = mcolors.LinearSegmentedColormap.from_list(
    'nws_dbz',
    list(zip(np.array(DBZ_BOUNDS) / 75.0, DBZ_COLORS))
)
DBZ_NORM = mcolors.BoundaryNorm(DBZ_BOUNDS, len(DBZ_COLORS))

class RadarState:
    def __init__(self):
        self.lock        = threading.Lock()
        self.sweep_dbz   = np.zeros(181)
        self.scan_angle  = 0
        self.pitch       = 0.0
        self.roll        = 0.0
        self.lat         = 0.0
        self.lon         = 0.0
        self.alt_ft      = 0.0
        self.spd_kt      = 0.0
        self.hdg_deg     = 0.0
        self.connected   = False
        self.last_update = 0.0

    def update_from_json(self, data: dict):
        with self.lock:
            if 'sweep' in data and len(data['sweep']) == 181:
                new = np.array(data['sweep'], dtype=float)
                self.sweep_dbz = np.where(new > 0, new,
                                          self.sweep_dbz * DBZ_DECAY)
            self.scan_angle = int(data.get('angle', self.scan_angle))
            self.pitch      = float(data.get('pitch', 0))
            self.roll       = float(data.get('roll',  0))
            self.lat        = float(data.get('lat',   0))
            self.lon        = float(data.get('lon',   0))
            self.alt_ft     = float(data.get('alt',   0))
            self.spd_kt     = float(data.get('spd',   0))
            self.hdg_deg    = float(data.get('hdg',   0))
            self.connected  = True
            self.last_update = time.time()

    def snapshot(self):
        with self.lock:
            return (
                self.sweep_dbz.copy(),
                self.scan_angle,
                self.pitch, self.roll,
                self.lat, self.lon,
                self.alt_ft, self.spd_kt, self.hdg_deg,
                self.connected,
            )

state = RadarState()

def demo_thread():
    t = 0.0
    while True:
        dbz_arr = np.zeros(181)
        for deg in range(45, 75):
            dbz_arr[deg] = max(0, 55 + 10 * math.sin(t + deg * 0.2)
                               + np.random.normal(0, 3))
        for deg in range(110, 141):
            dbz_arr[deg] = max(0, 28 + 5 * math.cos(t * 0.7 + deg * 0.1)
                               + np.random.normal(0, 2))
        scan = int(((math.sin(t * 1.2) + 1) / 2) * 180)
        with state.lock:
            state.sweep_dbz = np.where(dbz_arr > 0, dbz_arr,
                                        state.sweep_dbz * DBZ_DECAY)
            state.scan_angle  = scan
            state.pitch       = 2.0 * math.sin(t * 0.3)
            state.roll        = 1.5 * math.cos(t * 0.2)
            state.lat         = 37.6213
            state.lon         = -122.3790
            state.alt_ft      = 10500.0
            state.spd_kt      = 245.0
            state.hdg_deg     = 270.0
            state.connected   = True
            state.last_update = time.time()
        t += 0.05
        time.sleep(0.05)

def fetch_thread(host: str):
    url = f"http://{host}/radardata"
    session = requests.Session()
    while True:
        try:
            r = session.get(url, timeout=FETCH_TIMEOUT)
            if r.status_code == 200:
                state.update_from_json(r.json())
        except Exception:
            with state.lock:
                state.connected = False
        time.sleep(1.0 / POLL_HZ)

def build_figure():
    fig = plt.figure("WxRadar — In-Flight Weather", facecolor='#0a0a0f',
                     figsize=(14, 7))
    fig.set_dpi(100)

    ax_radar = fig.add_axes([0.01, 0.05, 0.58, 0.90],
                            polar=True, facecolor='#000a00')
    ax_radar.set_theta_zero_location('W')
    ax_radar.set_theta_direction(-1)
    ax_radar.set_thetamin(0)
    ax_radar.set_thetamax(180)
    ax_radar.set_ylim(0, MAX_RANGE_NM)
    ax_radar.set_yticks([25, 50, 75, 100])
    ax_radar.set_yticklabels(['25NM', '50NM', '75NM', '100NM'],
                             color='#2a7a2a', fontsize=7)
    ax_radar.set_thetagrids(np.arange(0, 181, 30),
                            [f'{d}°' for d in range(0, 181, 30)],
                            color='#2a7a2a', fontsize=8)
    ax_radar.tick_params(colors='#2a7a2a')
    ax_radar.grid(color='#1a4a1a', linestyle='--', alpha=0.5)
    ax_radar.spines['polar'].set_color('#1a4a1a')

    theta_arr = np.arange(0, 181, 1) * np.pi / 180.0
    r_zeros   = np.zeros(181)
    scatter, = ax_radar.plot(theta_arr, r_zeros,
                             'o', ms=3, color='#62f51f', alpha=0)

    sweep_line, = ax_radar.plot([0, 0], [0, MAX_RANGE_NM],
                                color='#62f51f', linewidth=2, alpha=0.9)

    ax_att = fig.add_axes([0.62, 0.55, 0.17, 0.38], facecolor='#050f05')
    ax_att.set_xlim(-1, 1)
    ax_att.set_ylim(-1, 1)
    ax_att.set_aspect('equal')
    ax_att.axis('off')
    ax_att.set_title('ATTITUDE', color='#62f51f', fontsize=8)

    ax_data = fig.add_axes([0.62, 0.04, 0.36, 0.48], facecolor='#050f05')
    ax_data.axis('off')

    ax_cb = fig.add_axes([0.60, 0.94, 0.38, 0.04])
    sm = cm.ScalarMappable(cmap=DBZ_CMAP, norm=mcolors.Normalize(0, 75))
    sm.set_array([])
    cb = plt.colorbar(sm, cax=ax_cb, orientation='horizontal')
    cb.set_label('Reflectivity (dBZ)', color='#62f51f', fontsize=8)
    cb.ax.xaxis.set_tick_params(color='#62f51f', labelcolor='#62f51f', labelsize=7)
    ax_cb.set_facecolor('#050f05')

    return fig, ax_radar, sweep_line, ax_att, ax_data

def draw_attitude(ax, pitch_deg, roll_deg):
    ax.cla()
    ax.set_xlim(-1, 1)
    ax.set_ylim(-1, 1)
    ax.set_aspect('equal')
    ax.axis('off')
    ax.set_title('ATTITUDE', color='#62f51f', fontsize=8, pad=2)

    pitch_norm = np.clip(pitch_deg / 30.0, -1, 1)

    t = np.linspace(0, 2 * np.pi, 100)
    roll_r = np.deg2rad(roll_deg)

    for side, col in [(1, '#1a3a6a'), (-1, '#5a3010')]:
        xs = np.array([-2, 2, 2, -2, -2])
        ys = np.array([0, 0, side * 2, side * 2, 0]) - pitch_norm
        xr = xs * math.cos(roll_r) - ys * math.sin(roll_r)
        yr = xs * math.sin(roll_r) + ys * math.cos(roll_r)
        ax.fill(xr, yr, color=col, clip_on=True, zorder=1)

    hx = np.array([-1, 1])
    hy = np.array([-pitch_norm, -pitch_norm])
    xr = hx * math.cos(roll_r) - hy * math.sin(roll_r)
    yr = hx * math.sin(roll_r) + hy * math.cos(roll_r)
    ax.plot(xr, yr, color='white', linewidth=1.5, zorder=2)

    ax.plot([-0.35, 0, 0.35], [0, 0, 0], color='yellow', linewidth=2, zorder=3)
    ax.plot([0, 0], [0, 0.1], color='yellow', linewidth=2, zorder=3)
    ax.set_xlim(-1, 1)
    ax.set_ylim(-1, 1)

    ax.text(0, -0.88, f'P {pitch_deg:+.1f}°  R {roll_deg:+.1f}°',
            ha='center', color='#aaffaa', fontsize=7, zorder=4)

def draw_data_panel(ax, state_snap):
    (_, _, pitch, roll, lat, lon, alt, spd, hdg, connected) = state_snap
    ax.cla()
    ax.axis('off')
    ax.set_facecolor('#050f05')

    lines = [
        ('STATUS',  'LIVE' if connected else 'NO SIGNAL',
         '#62f51f' if connected else '#ff4444'),
        ('HDG',     f'{hdg:06.2f}°',   '#ffffff'),
        ('ALT',     f'{alt:,.0f} ft',  '#ffffff'),
        ('SPD',     f'{spd:.1f} kt',   '#ffffff'),
        ('LAT',     f'{lat:.4f}°',     '#aaffaa'),
        ('LON',     f'{lon:.4f}°',     '#aaffaa'),
    ]
    for i, (label, val, col) in enumerate(lines):
        y = 0.92 - i * 0.16
        ax.text(0.05, y, label, transform=ax.transAxes,
                color='#446644', fontsize=9, va='top', fontfamily='monospace')
        ax.text(0.50, y, val,   transform=ax.transAxes,
                color=col,      fontsize=9, va='top', fontfamily='monospace',
                fontweight='bold')

def draw_sweep_returns(ax_radar, dbz_arr):
    while len(ax_radar.collections) > 0:
        ax_radar.collections[0].remove()

    for deg in range(0, 181):
        dbz = dbz_arr[deg]
        if dbz < 5.0:
            continue
        r_nm = (dbz / 75.0) * MAX_RANGE_NM
        theta = deg * np.pi / 180.0
        half  = 0.9 * np.pi / 180.0

        color = DBZ_CMAP(dbz / 75.0)
        alpha = min(0.9, 0.4 + dbz / 100.0)

        t_range = np.linspace(theta - half, theta + half, 3)
        ax_radar.fill_between(t_range, 0, r_nm,
                              color=color, alpha=alpha, linewidth=0)

def main(host: str, demo: bool):
    fig, ax_radar, sweep_line, ax_att, ax_data = build_figure()

    plt.ion()
    plt.show(block=False)

    theta_arr = np.arange(0, 181) * np.pi / 180.0

    while plt.fignum_exists(fig.number):
        snap = state.snapshot()
        (dbz_arr, scan_ang, pitch, roll,
         lat, lon, alt, spd, hdg, connected) = snap

        draw_sweep_returns(ax_radar, dbz_arr)

        theta_sweep = scan_ang * np.pi / 180.0
        sweep_line.set_data([theta_sweep, theta_sweep], [0, MAX_RANGE_NM])

        draw_attitude(ax_att, pitch, roll)

        draw_data_panel(ax_data, snap)

        fig.canvas.draw_idle()
        fig.canvas.flush_events()
        time.sleep(1.0 / POLL_HZ)

    plt.close('all')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='WxRadar Cockpit Display')
    parser.add_argument('--host',  default='192.168.4.1',
                        help='ESP32 IP address (default: 192.168.4.1)')
    parser.add_argument('--demo',  action='store_true',
                        help='Run with synthetic weather (no hardware)')
    args = parser.parse_args()

    if args.demo:
        print("Demo mode — synthetic weather cells active")
        t = threading.Thread(target=demo_thread, daemon=True)
    else:
        print(f"Connecting to ESP32 at {args.host} …")
        t = threading.Thread(target=fetch_thread, args=(args.host,), daemon=True)
    t.start()

    main(args.host, args.demo)