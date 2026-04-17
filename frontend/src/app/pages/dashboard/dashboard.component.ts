import { Component, inject, signal, OnInit, OnDestroy } from '@angular/core';
import { RouterLink } from '@angular/router';
import { ApiService } from '../../services/api.service';

@Component({
  selector: 'app-dashboard',
  standalone: true,
  imports: [RouterLink],
  template: `
    <div class="card">
      <h1>Dashboard</h1>
      <p class="subtitle">Colada Lightning — Fire TV Control</p>
    </div>

    @if (loading()) {
      <div class="card"><p class="muted">Loading...</p></div>
    } @else {
      <div class="grid">
        <div class="card stat-card">
          <div class="stat-icon" [class.connected]="dbConnected()">DB</div>
          <div class="stat-label">Database</div>
          <div class="stat-value">{{ dbConnected() ? 'Connected' : 'Disconnected' }}</div>
        </div>
        <div class="card stat-card">
          <div class="stat-icon" [class.connected]="mqttConnected()">MQ</div>
          <div class="stat-label">MQTT</div>
          <div class="stat-value">{{ mqttConnected() ? 'Connected' : 'Disconnected' }}</div>
        </div>
        <div class="card stat-card">
          <div class="stat-icon num">{{ totalDevices() }}</div>
          <div class="stat-label">Devices</div>
          <div class="stat-value">{{ onlineDevices() }} online</div>
        </div>
        <div class="card stat-card">
          <div class="stat-icon num">{{ pairedDevices() }}</div>
          <div class="stat-label">Paired</div>
          <div class="stat-value">{{ totalDevices() - pairedDevices() }} unpaired</div>
        </div>
      </div>

      @if (devices().length > 0) {
        <div class="card">
          <h2>Your Devices</h2>
          <table>
            <thead>
              <tr>
                <th>Status</th>
                <th>Device</th>
                <th>IP Address</th>
                <th>Paired</th>
                <th>Last Seen</th>
              </tr>
            </thead>
            <tbody>
              @for (d of devices(); track d.device_id) {
                <tr>
                  <td><span class="dot" [class.dot-online]="d.status === 'online'" [class.dot-offline]="d.status === 'offline'" [class.dot-pairing]="d.status === 'pairing'"></span></td>
                  <td>
                    <div class="device-name">{{ d.name }}</div>
                    <div class="device-id">{{ d.device_id }}</div>
                  </td>
                  <td class="muted">{{ d.ip_address }}</td>
                  <td>
                    <span class="badge" [class.badge-success]="d.is_paired" [class.badge-gray]="!d.is_paired">
                      {{ d.is_paired ? 'Yes' : 'No' }}
                    </span>
                  </td>
                  <td class="muted">{{ d.last_seen_at || 'Never' }}</td>
                </tr>
              }
            </tbody>
          </table>
        </div>
      } @else {
        <div class="card empty-state">
          <p>No devices configured.</p>
          <a routerLink="/devices" class="btn btn-primary">Add Device</a>
        </div>
      }

      <div class="grid actions-grid">
        <a routerLink="/devices" class="card action-card">
          <div class="action-title">Manage Devices</div>
          <div class="action-desc">Add, edit, or pair devices</div>
        </a>
        <a routerLink="/remote" class="card action-card">
          <div class="action-title">Remote Control</div>
          <div class="action-desc">Control your Fire TV</div>
        </a>
        <a routerLink="/apps" class="card action-card">
          <div class="action-title">Manage Apps</div>
          <div class="action-desc">Configure device apps</div>
        </a>
      </div>
    }
  `,
  styles: [`
    h1 { font-size: 22px; font-weight: 600; }
    h2 { font-size: 18px; font-weight: 600; margin-bottom: 16px; }
    .subtitle { color: var(--text-muted); font-size: 14px; margin-top: 4px; }
    .grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 0; }
    .stat-card { text-align: center; }
    .stat-icon { font-size: 28px; font-weight: 700; color: var(--error); margin-bottom: 8px; }
    .stat-icon.connected { color: var(--success); }
    .stat-icon.num { color: var(--accent); }
    .stat-label { font-size: 13px; color: var(--text-muted); text-transform: uppercase; letter-spacing: 0.5px; }
    .stat-value { font-size: 14px; color: var(--text); margin-top: 4px; }
    .device-name { font-weight: 500; }
    .device-id { font-size: 12px; color: var(--text-muted); }
    .muted { color: var(--text-muted); }
    .empty-state { text-align: center; padding: 40px; }
    .empty-state p { margin-bottom: 16px; color: var(--text-muted); }
    .actions-grid { grid-template-columns: repeat(3, 1fr); }
    .action-card {
      text-decoration: none; color: var(--text);
      cursor: pointer; transition: border-color 0.2s;
      border: 1px solid transparent;
    }
    .action-card:hover { border-color: var(--accent); }
    .action-title { font-size: 16px; font-weight: 600; color: var(--accent); margin-bottom: 4px; }
    .action-desc { font-size: 13px; color: var(--text-muted); }
  `],
})
export class DashboardComponent implements OnInit, OnDestroy {
  private api = inject(ApiService);
  private timer: ReturnType<typeof setInterval> | null = null;

  loading = signal(true);
  dbConnected = signal(false);
  mqttConnected = signal(false);
  totalDevices = signal(0);
  pairedDevices = signal(0);
  onlineDevices = signal(0);
  devices = signal<any[]>([]);

  ngOnInit() {
    this.load();
    this.timer = setInterval(() => this.load(), 15000);
  }

  ngOnDestroy() {
    if (this.timer) clearInterval(this.timer);
  }

  load() {
    this.api.getStatus().subscribe({
      next: (status) => {
        this.dbConnected.set(status.connections?.database === 'connected');
        this.mqttConnected.set(status.connections?.mqtt === 'connected');
        this.totalDevices.set(status.devices?.total ?? 0);
        this.pairedDevices.set(status.devices?.paired ?? 0);
        this.onlineDevices.set(status.devices?.online ?? 0);
      },
      error: () => {},
    });
    this.api.getDevices().subscribe({
      next: (res) => {
        this.devices.set(res.devices || []);
        this.loading.set(false);
      },
      error: () => this.loading.set(false),
    });
  }
}
