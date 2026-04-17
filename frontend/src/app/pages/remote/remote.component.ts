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
  'com.apple.atve.androidtv.appletv': '#555',
};

@Component({
  selector: 'app-remote',
  standalone: true,
  imports: [FormsModule],
  template: `
    <div class="card">
      <div class="header">
        <h1>Remote Control</h1>
        <select [(ngModel)]="selectedDevice" (ngModelChange)="onDeviceChange()" class="device-select">
          <option value="">Select device...</option>
          @for (d of devices(); track d.device_id) {
            <option [value]="d.device_id">{{ d.name }}</option>
          }
        </select>
      </div>
    </div>

    @if (selectedDevice) {
      @if (favoriteApps().length > 0) {
        <div class="card">
          <h3>Quick Launch</h3>
          <div class="app-row">
            @for (app of favoriteApps(); track app.package) {
              <button class="app-btn" [style.background]="getColor(app.package)" (click)="launchApp(app)">
                {{ app.name }}
              </button>
            }
          </div>
        </div>
      }

      <div class="remote-layout">
        <div class="card remote-pad">
          <div class="pad-row">
            <button class="pad-btn" (click)="nav('dpad_up')">&#9650;</button>
          </div>
          <div class="pad-row">
            <button class="pad-btn" (click)="nav('dpad_left')">&#9664;</button>
            <button class="pad-btn select-btn" (click)="nav('select')">OK</button>
            <button class="pad-btn" (click)="nav('dpad_right')">&#9654;</button>
          </div>
          <div class="pad-row">
            <button class="pad-btn" (click)="nav('dpad_down')">&#9660;</button>
          </div>
          <div class="pad-row nav-row">
            <button class="btn btn-secondary" (click)="nav('back')">Back</button>
            <button class="btn btn-secondary" (click)="nav('home')">Home</button>
            <button class="btn btn-secondary" (click)="nav('menu')">Menu</button>
          </div>
        </div>

        <div class="card">
          <h3>Power</h3>
          <div class="btn-group">
            <button class="btn btn-primary" (click)="nav('wake')">Wake</button>
            <button class="btn btn-secondary" (click)="nav('sleep')">Sleep</button>
          </div>
        </div>

        <div class="card">
          <h3>Media</h3>
          <div class="btn-group">
            <button class="btn btn-secondary" (click)="media('play')">Play</button>
            <button class="btn btn-secondary" (click)="media('pause')">Pause</button>
            <button class="btn btn-secondary" (click)="media('scan_forward')">FF</button>
            <button class="btn btn-secondary" (click)="media('scan_backward')">RW</button>
          </div>
        </div>

        <div class="card">
          <h3>Volume</h3>
          <div class="btn-group">
            <button class="btn btn-secondary" (click)="nav('volume_up')">Vol +</button>
            <button class="btn btn-secondary" (click)="nav('volume_down')">Vol -</button>
            <button class="btn btn-secondary" (click)="nav('volume_mute')">Mute</button>
          </div>
        </div>

        <div class="card">
          <h3>Keyboard</h3>
          <div class="keyboard-row">
            <input type="text" [(ngModel)]="textInput" placeholder="Type text..." (keydown.enter)="sendText()">
            <button class="btn btn-primary" (click)="sendText()" [disabled]="!textInput">Send</button>
          </div>
        </div>
      </div>

      @if (feedback()) {
        <div class="card feedback" [class.error]="feedbackError()">{{ feedback() }}</div>
      }
    } @else {
      <div class="card empty">
        <p class="muted">Select a device to control</p>
      </div>
    }
  `,
  styles: [`
    .header { display: flex; justify-content: space-between; align-items: center; }
    h1 { font-size: 22px; font-weight: 600; }
    h3 { font-size: 15px; font-weight: 600; color: var(--accent); margin-bottom: 12px; text-transform: uppercase; letter-spacing: 0.5px; }
    .device-select { width: auto; min-width: 200px; }
    .app-row { display: flex; gap: 8px; flex-wrap: wrap; }
    .app-btn {
      padding: 10px 18px; border: none; border-radius: 8px;
      color: #fff; font-weight: 600; font-size: 13px;
      cursor: pointer; transition: opacity 0.15s, transform 0.1s;
    }
    .app-btn:hover { opacity: 0.85; }
    .app-btn:active { transform: scale(0.95); }
    .remote-layout { display: grid; grid-template-columns: 1fr 1fr; gap: 0; }
    .remote-pad { grid-row: span 2; display: flex; flex-direction: column; align-items: center; gap: 8px; }
    .pad-row { display: flex; gap: 8px; align-items: center; }
    .pad-btn {
      width: 56px; height: 56px; border-radius: 50%; border: 1px solid var(--border);
      background: var(--bg-hover); color: var(--text); font-size: 20px;
      cursor: pointer; transition: background 0.15s;
      display: flex; align-items: center; justify-content: center;
    }
    .pad-btn:hover { background: var(--accent); color: #000; }
    .pad-btn:active { transform: scale(0.95); }
    .select-btn { background: var(--accent); color: #000; font-weight: 700; font-size: 14px; }
    .nav-row { margin-top: 16px; }
    .btn-group { display: flex; gap: 8px; flex-wrap: wrap; }
    .keyboard-row { display: flex; gap: 8px; }
    .keyboard-row input { flex: 1; }
    .muted { color: var(--text-muted); }
    .empty { text-align: center; padding: 40px; }
    .feedback { font-size: 13px; color: var(--success); }
    .feedback.error { color: var(--error); }
  `],
})
export class RemoteComponent implements OnInit {
  private api = inject(ApiService);
  devices = signal<any[]>([]);
  favoriteApps = signal<any[]>([]);
  selectedDevice = '';
  textInput = '';
  feedback = signal('');
  feedbackError = signal(false);

  ngOnInit() {
    this.api.getDevices().subscribe({
      next: (res) => this.devices.set(res.devices || []),
    });
  }

  onDeviceChange() {
    this.favoriteApps.set([]);
    if (!this.selectedDevice) return;
    this.api.getApps(this.selectedDevice).subscribe({
      next: (res) => {
        const apps = res.apps || res || [];
        this.favoriteApps.set(apps.filter((a: any) => a.is_favorite));
      },
    });
  }

  getColor(pkg: string): string {
    return APP_COLORS[pkg] || 'var(--accent)';
  }

  launchApp(app: any) {
    this.api.launchApp(this.selectedDevice, app.package).subscribe({
      next: () => this.showFeedback('Launched ' + app.name),
      error: (e) => this.showFeedback(e.error?.detail || 'Launch failed', true),
    });
  }

  nav(action: string) {
    if (!this.selectedDevice) return;
    this.api.sendNavigation(this.selectedDevice, action).subscribe({
      next: () => this.showFeedback('Command sent'),
      error: (e) => this.showFeedback(e.error?.detail || 'Command failed', true),
    });
  }

  media(action: string) {
    if (!this.selectedDevice) return;
    this.api.sendMedia(this.selectedDevice, action).subscribe({
      next: () => this.showFeedback('Command sent'),
      error: (e) => this.showFeedback(e.error?.detail || 'Command failed', true),
    });
  }

  sendText() {
    if (!this.selectedDevice || !this.textInput) return;
    this.api.sendKeyboard(this.selectedDevice, this.textInput).subscribe({
      next: () => { this.showFeedback('Text sent'); this.textInput = ''; },
      error: (e) => this.showFeedback(e.error?.detail || 'Send failed', true),
    });
  }

  private showFeedback(msg: string, isError = false) {
    this.feedback.set(msg);
    this.feedbackError.set(isError);
    setTimeout(() => this.feedback.set(''), 2000);
  }
}
