import { Component, inject, signal, OnInit } from '@angular/core';
import { ApiService } from '../../services/api.service';

@Component({
  selector: 'app-settings',
  standalone: true,
  template: `
    <div class="card">
      <h1>Settings</h1>
    </div>

    <div class="card">
      <h3>Service Status</h3>
      @if (loading()) {
        <p class="muted">Loading...</p>
      } @else {
        <div class="info-grid">
          <div class="info-row">
            <span class="info-label">Service</span>
            <span>{{ status().service }} v{{ status().version }}</span>
          </div>
          <div class="info-row">
            <span class="info-label">Database</span>
            <span class="badge" [class.badge-success]="status().connections?.database === 'connected'" [class.badge-danger]="status().connections?.database !== 'connected'">
              {{ status().connections?.database }}
            </span>
          </div>
          <div class="info-row">
            <span class="info-label">MQTT</span>
            <span class="badge" [class.badge-success]="status().connections?.mqtt === 'connected'" [class.badge-danger]="status().connections?.mqtt !== 'connected'">
              {{ status().connections?.mqtt }}
            </span>
          </div>
          <div class="info-row">
            <span class="info-label">DB Host</span>
            <span class="muted">{{ status().config?.db_host }}</span>
          </div>
          <div class="info-row">
            <span class="info-label">MQTT Broker</span>
            <span class="muted">{{ status().config?.mqtt_broker }}</span>
          </div>
        </div>
      }
    </div>

    <div class="card">
      <h3>Health</h3>
      @if (health()) {
        <div class="info-grid">
          <div class="info-row">
            <span class="info-label">Overall</span>
            <span class="badge" [class.badge-success]="health().status === 'healthy'" [class.badge-warning]="health().status !== 'healthy'">
              {{ health().status }}
            </span>
          </div>
        </div>
      }
    </div>

    <div class="card">
      <h3>About</h3>
      <div class="info-grid">
        <div class="info-row">
          <span class="info-label">Protocol</span>
          <span class="muted">Fire TV Lightning API</span>
        </div>
        <div class="info-row">
          <span class="info-label">Integration</span>
          <span class="muted">MQTT Discovery (Home Assistant)</span>
        </div>
        <div class="info-row">
          <span class="info-label">Backend</span>
          <span class="muted">C++ / Drogon</span>
        </div>
      </div>
    </div>
  `,
  styles: [`
    h1 { font-size: 22px; font-weight: 600; }
    h3 { font-size: 15px; font-weight: 600; color: var(--accent); margin-bottom: 16px; text-transform: uppercase; letter-spacing: 0.5px; }
    .muted { color: var(--text-muted); }
    .info-grid { display: flex; flex-direction: column; gap: 12px; }
    .info-row { display: flex; justify-content: space-between; align-items: center; padding: 8px 0; border-bottom: 1px solid var(--border); }
    .info-row:last-child { border-bottom: none; }
    .info-label { color: var(--text-muted); font-size: 13px; }
  `],
})
export class SettingsComponent implements OnInit {
  private api = inject(ApiService);

  loading = signal(true);
  status = signal<any>({});
  health = signal<any>(null);

  ngOnInit() {
    this.api.getStatus().subscribe({
      next: (s) => { this.status.set(s); this.loading.set(false); },
      error: () => this.loading.set(false),
    });
    this.api.getHealth().subscribe({
      next: (h) => this.health.set(h),
    });
  }
}
