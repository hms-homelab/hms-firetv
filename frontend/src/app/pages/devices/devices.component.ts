import { Component, inject, signal, OnInit, OnDestroy } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { ApiService } from '../../services/api.service';

@Component({
  selector: 'app-devices',
  standalone: true,
  imports: [FormsModule],
  template: `
    <div class="card">
      <div class="header">
        <h1>Devices</h1>
        <button class="btn btn-primary" (click)="openAdd()">Add Device</button>
      </div>

      <table>
        <thead>
          <tr>
            <th>Status</th>
            <th>Device</th>
            <th>IP Address</th>
            <th>Paired</th>
            <th>ADB</th>
            <th>Actions</th>
          </tr>
        </thead>
        <tbody>
          @for (d of devices(); track d.device_id) {
            <tr>
              <td><span class="dot" [class.dot-online]="d.status === 'online'" [class.dot-offline]="d.status === 'offline'" [class.dot-pairing]="d.status === 'pairing'"></span></td>
              <td>
                <a class="device-link" (click)="openEdit(d)">{{ d.name }}</a>
                <div class="device-id">{{ d.device_id }}</div>
              </td>
              <td class="muted">{{ d.ip_address }}</td>
              <td>
                <span class="badge" [class.badge-success]="d.is_paired" [class.badge-gray]="!d.is_paired">
                  {{ d.is_paired ? 'Yes' : 'No' }}
                </span>
              </td>
              <td class="muted">{{ d.adb_enabled ? 'On' : 'Off' }}</td>
              <td>
                <div class="actions">
                  @if (!d.is_paired) {
                    <button class="btn btn-primary btn-sm" (click)="startPair(d)">Pair</button>
                  } @else {
                    <button class="btn btn-secondary btn-sm" (click)="unpair(d)">Unpair</button>
                  }
                  <button class="btn btn-danger btn-sm" (click)="remove(d)">Delete</button>
                </div>
              </td>
            </tr>
          } @empty {
            <tr><td colspan="6" class="empty">No devices. Click "Add Device" to get started.</td></tr>
          }
        </tbody>
      </table>
    </div>

    @if (showForm()) {
      <div class="modal-overlay" (click)="onOverlay($event)">
        <div class="modal">
          <h2>{{ editDevice() ? 'Edit Device' : 'Add Device' }}</h2>
          <div class="form-group">
            <label>Device ID</label>
            <input type="text" [(ngModel)]="form.device_id" placeholder="living_room" [disabled]="!!editDevice()">
          </div>
          <div class="form-group">
            <label>Name</label>
            <input type="text" [(ngModel)]="form.name" placeholder="Living Room Fire TV">
          </div>
          <div class="form-group">
            <label>IP Address</label>
            <input type="text" [(ngModel)]="form.ip_address" placeholder="192.168.2.xxx">
          </div>
          @if (error()) { <div class="error-msg">{{ error() }}</div> }
          <div class="btn-row">
            <button class="btn btn-secondary" (click)="closeForm()">Cancel</button>
            <button class="btn btn-primary" (click)="save()" [disabled]="saving()">
              {{ saving() ? 'Saving...' : 'Save' }}
            </button>
          </div>
        </div>
      </div>
    }

    @if (showPairModal()) {
      <div class="modal-overlay" (click)="onOverlay($event)">
        <div class="modal">
          <h2>Pair: {{ pairDevice()?.name }}</h2>
          @if (pairStep() === 'waiting') {
            <p class="muted">A PIN should appear on your TV screen.</p>
            <div class="form-group" style="margin-top: 16px;">
              <label>Enter PIN</label>
              <input type="text" [(ngModel)]="pairPin" placeholder="Enter PIN from TV">
            </div>
            @if (error()) { <div class="error-msg">{{ error() }}</div> }
            <div class="btn-row">
              <button class="btn btn-secondary" (click)="closePair()">Cancel</button>
              <button class="btn btn-primary" (click)="verifyPin()" [disabled]="!pairPin || saving()">Verify</button>
            </div>
          } @else {
            <div class="success-msg">Device paired successfully!</div>
            <div class="btn-row" style="margin-top: 16px;">
              <button class="btn btn-primary" (click)="closePair()">Done</button>
            </div>
          }
        </div>
      </div>
    }
  `,
  styles: [`
    .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; }
    h1 { font-size: 22px; font-weight: 600; }
    .device-link { color: var(--accent); cursor: pointer; text-decoration: none; }
    .device-link:hover { text-decoration: underline; }
    .device-id { font-size: 12px; color: var(--text-muted); }
    .muted { color: var(--text-muted); }
    .actions { display: flex; gap: 4px; }
    .empty { text-align: center; color: var(--text-muted); padding: 40px 0 !important; }
    .btn-row { display: flex; justify-content: flex-end; gap: 8px; margin-top: 20px; }
  `],
})
export class DevicesComponent implements OnInit, OnDestroy {
  private api = inject(ApiService);
  private timer: ReturnType<typeof setInterval> | null = null;

  devices = signal<any[]>([]);
  showForm = signal(false);
  editDevice = signal<any>(null);
  saving = signal(false);
  error = signal('');

  showPairModal = signal(false);
  pairDevice = signal<any>(null);
  pairStep = signal<'waiting' | 'done'>('waiting');
  pairPin = '';

  form: any = { device_id: '', name: '', ip_address: '' };

  ngOnInit() {
    this.loadDevices();
    this.timer = setInterval(() => this.loadDevices(), 10000);
  }

  ngOnDestroy() { if (this.timer) clearInterval(this.timer); }

  loadDevices() {
    this.api.getDevices().subscribe({
      next: (res) => this.devices.set(res.devices || []),
    });
  }

  openAdd() {
    this.form = { device_id: '', name: '', ip_address: '' };
    this.editDevice.set(null);
    this.error.set('');
    this.showForm.set(true);
  }

  openEdit(d: any) {
    this.form = { device_id: d.device_id, name: d.name, ip_address: d.ip_address };
    this.editDevice.set(d);
    this.error.set('');
    this.showForm.set(true);
  }

  closeForm() { this.showForm.set(false); this.editDevice.set(null); }

  save() {
    if (!this.form.device_id || !this.form.name || !this.form.ip_address) {
      this.error.set('All fields are required.');
      return;
    }
    this.saving.set(true);
    this.error.set('');
    const obs = this.editDevice()
      ? this.api.updateDevice(this.form.device_id, this.form)
      : this.api.createDevice(this.form);
    obs.subscribe({
      next: () => { this.saving.set(false); this.closeForm(); this.loadDevices(); },
      error: (e) => { this.saving.set(false); this.error.set(e.error?.detail || 'Failed to save.'); },
    });
  }

  remove(d: any) {
    if (confirm(`Delete "${d.name}"? This cannot be undone.`)) {
      this.api.deleteDevice(d.device_id).subscribe({ next: () => this.loadDevices() });
    }
  }

  startPair(d: any) {
    this.pairDevice.set(d);
    this.pairStep.set('waiting');
    this.pairPin = '';
    this.error.set('');
    this.showPairModal.set(true);
    this.api.startPairing(d.device_id).subscribe({
      error: (e) => this.error.set(e.error?.detail || 'Failed to start pairing.'),
    });
  }

  verifyPin() {
    const d = this.pairDevice();
    if (!d || !this.pairPin) return;
    this.saving.set(true);
    this.error.set('');
    this.api.verifyPairing(d.device_id, this.pairPin).subscribe({
      next: () => { this.saving.set(false); this.pairStep.set('done'); this.loadDevices(); },
      error: (e) => { this.saving.set(false); this.error.set(e.error?.detail || 'PIN verification failed.'); },
    });
  }

  unpair(d: any) {
    if (confirm(`Unpair "${d.name}"?`)) {
      this.api.resetPairing(d.device_id).subscribe({ next: () => this.loadDevices() });
    }
  }

  closePair() { this.showPairModal.set(false); this.pairDevice.set(null); }

  onOverlay(e: MouseEvent) {
    if ((e.target as HTMLElement).classList.contains('modal-overlay')) {
      this.closeForm();
      this.closePair();
    }
  }
}
