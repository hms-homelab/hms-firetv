import { Component, inject, signal, OnInit } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { ApiService } from '../../services/api.service';

const APP_COLORS: Record<string, string> = {
  'com.netflix.ninja': '#e50914',
  'com.amazon.avod.thirdpartyclient': '#00a8e1',
  'com.disney.disneyplus': '#113ccf',
  'com.google.android.youtube.tv': '#ff0000',
  'com.hulu.plus': '#1ce783',
  'com.hbo.hbonow': '#b535f6',
  'com.spotify.tv.android': '#1db954',
  'com.plexapp.android': '#e5a00d',
  'com.apple.atve.androidtv.appletv': '#000000',
};

const APP_LABELS: Record<string, string> = {
  'com.netflix.ninja': 'N',
  'com.amazon.avod.thirdpartyclient': 'P',
  'com.disney.disneyplus': 'D+',
  'com.google.android.youtube.tv': 'YT',
  'com.hulu.plus': 'H',
  'com.hbo.hbonow': 'MAX',
  'com.spotify.tv.android': 'S',
  'com.plexapp.android': 'PX',
  'com.apple.atve.androidtv.appletv': 'TV',
};

@Component({
  selector: 'app-apps',
  standalone: true,
  imports: [FormsModule],
  template: `
    <div class="card">
      <div class="header">
        <h1>Apps</h1>
        <select [(ngModel)]="selectedDevice" (ngModelChange)="onDeviceChange()">
          <option value="">Select device...</option>
          @for (d of devices(); track d.device_id) {
            <option [value]="d.device_id">{{ d.name }}</option>
          }
        </select>
      </div>
    </div>

    @if (selectedDevice) {
      <div class="card">
        <div class="header">
          <h2>Installed Apps</h2>
          <button class="btn btn-primary btn-sm" (click)="showAddForm.set(true)">Add App</button>
        </div>

        @if (apps().length > 0) {
          <div class="app-grid">
            @for (app of apps(); track app.package) {
              <div class="app-card">
                <div class="app-icon" [style.background]="getColor(app.package)">
                  {{ getLabel(app.package) }}
                </div>
                <div class="app-info">
                  <div class="app-name">{{ app.name }}</div>
                  <div class="app-pkg">{{ app.package }}</div>
                </div>
                <div class="app-actions">
                  <button class="btn btn-primary btn-sm" (click)="launch(app)">Launch</button>
                  <button class="btn btn-danger btn-sm" (click)="removeApp(app)">Remove</button>
                </div>
              </div>
            }
          </div>
        } @else {
          <p class="muted">No apps configured for this device.</p>
        }
      </div>

      @if (showAddForm()) {
        <div class="modal-overlay" (click)="onOverlay($event)">
          <div class="modal">
            <h2>Add App</h2>
            <div class="form-group">
              <label>App Name</label>
              <input type="text" [(ngModel)]="newApp.name" placeholder="Netflix">
            </div>
            <div class="form-group">
              <label>Package Name</label>
              <input type="text" [(ngModel)]="newApp.package_name" placeholder="com.netflix.ninja">
            </div>
            @if (error()) { <div class="error-msg">{{ error() }}</div> }
            <div class="btn-row">
              <button class="btn btn-secondary" (click)="showAddForm.set(false)">Cancel</button>
              <button class="btn btn-primary" (click)="addApp()">Add</button>
            </div>
          </div>
        </div>
      }

      @if (feedback()) {
        <div class="card feedback">{{ feedback() }}</div>
      }
    } @else {
      <div class="card empty"><p class="muted">Select a device to manage apps</p></div>
    }
  `,
  styles: [`
    .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 16px; }
    h1 { font-size: 22px; font-weight: 600; }
    h2 { font-size: 18px; font-weight: 600; }
    select { width: auto; min-width: 200px; }
    .app-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(220px, 1fr)); gap: 12px; }
    .app-card {
      background: var(--bg-primary); border-radius: 8px; padding: 16px;
      border: 1px solid var(--border); display: flex; flex-direction: column; gap: 12px;
    }
    .app-icon {
      width: 48px; height: 48px; border-radius: 10px;
      display: flex; align-items: center; justify-content: center;
      font-weight: 700; font-size: 16px; color: #fff;
      background: var(--accent);
    }
    .app-info { flex: 1; min-width: 0; }
    .app-name { font-weight: 600; font-size: 15px; }
    .app-pkg { font-size: 12px; color: var(--text-muted); word-break: break-all; margin-top: 2px; }
    .app-actions { display: flex; gap: 6px; }
    .muted { color: var(--text-muted); }
    .empty { text-align: center; padding: 40px; }
    .btn-row { display: flex; justify-content: flex-end; gap: 8px; margin-top: 20px; }
    .feedback { font-size: 13px; color: var(--success); }
  `],
})
export class AppsComponent implements OnInit {
  private api = inject(ApiService);

  devices = signal<any[]>([]);
  apps = signal<any[]>([]);
  selectedDevice = '';
  showAddForm = signal(false);
  error = signal('');
  feedback = signal('');
  newApp = { name: '', package_name: '' };

  ngOnInit() {
    this.api.getDevices().subscribe({
      next: (res) => this.devices.set(res.devices || []),
    });
  }

  onDeviceChange() {
    if (!this.selectedDevice) { this.apps.set([]); return; }
    this.loadApps();
  }

  loadApps() {
    this.api.getApps(this.selectedDevice).subscribe({
      next: (res) => this.apps.set(res.apps || res || []),
      error: () => this.apps.set([]),
    });
  }

  getColor(pkg: string): string {
    return APP_COLORS[pkg] || 'var(--accent)';
  }

  getLabel(pkg: string): string {
    return APP_LABELS[pkg] || pkg.split('.').pop()?.charAt(0).toUpperCase() || '?';
  }

  launch(app: any) {
    this.api.launchApp(this.selectedDevice, app.package).subscribe({
      next: () => this.showMsg('Launched ' + app.name),
      error: () => this.showMsg('Launch failed'),
    });
  }

  addApp() {
    if (!this.newApp.name || !this.newApp.package_name) {
      this.error.set('Both fields are required.');
      return;
    }
    this.api.addApp(this.selectedDevice, this.newApp).subscribe({
      next: () => {
        this.showAddForm.set(false);
        this.newApp = { name: '', package_name: '' };
        this.loadApps();
      },
      error: (e) => this.error.set(e.error?.detail || 'Failed to add app.'),
    });
  }

  removeApp(app: any) {
    if (confirm(`Remove "${app.name}"?`)) {
      this.api.deleteApp(this.selectedDevice, app.package).subscribe({
        next: () => this.loadApps(),
      });
    }
  }

  onOverlay(e: MouseEvent) {
    if ((e.target as HTMLElement).classList.contains('modal-overlay')) this.showAddForm.set(false);
  }

  private showMsg(msg: string) {
    this.feedback.set(msg);
    setTimeout(() => this.feedback.set(''), 2000);
  }
}
