import { Component, inject, signal, OnInit, OnDestroy } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { ApiService } from '../../services/api.service';

@Component({
  selector: 'app-devices',
  standalone: true,
  imports: [FormsModule],
  templateUrl: './devices.component.html',
  styleUrl: './devices.component.scss',
})
export class DevicesComponent implements OnInit, OnDestroy {
  private api = inject(ApiService);
  private timer: ReturnType<typeof setInterval> | null = null;

  devices = signal<any[]>([]);
  showForm = signal(false);
  editDevice = signal<any>(null);
  saving = signal(false);
  error = signal('');
  newDevices = signal<{ hostname: string; ip_address: string; has_wake_port: boolean; has_lightning: boolean }[]>([]);
  newDevice = signal(false);

  showPairModal = signal(false);
  pairDevice = signal<any>(null);
  pairStep = signal<'waiting' | 'done'>('waiting');
  pairPin = '';
  discovering = false;

  form: any = { device_id: '', name: '', ip_address: '' };

  ngOnInit() {
    this.loadDevices();
    this.timer = setInterval(() => this.loadDevices(), 10000);
  }

  ngOnDestroy() {
    if (this.timer) clearInterval(this.timer);
  }

  loadDevices() {
    this.api.getDevices().subscribe({
      next: (res) => this.devices.set(res.devices || []),
    });
  }

  openAdd() {
    this.form = { device_id: '', name: '', ip_address: '' };
    this.editDevice.set(null);
    this.error.set('');
    this.newDevice.set(false);
    this.showForm.set(true);
  }

  openEdit(d: any) {
    this.form = { device_id: d.device_id, name: d.name, ip_address: d.ip_address };
    this.editDevice.set(d);
    this.error.set('');
    this.showForm.set(true);
  }

  closeForm() {
    this.showForm.set(false);
    this.editDevice.set(null);
  }

  listNewDevices() {
    let self = this;
    if(!this.discovering){
      this.discovering =true;
      this.api.discover().subscribe({
        next: (res) => {
          self.newDevices.set(res.devices || []);
          self.discovering = false;
        },
        error: (err) => {},
      });

      this.showForm.set(false);
      this.editDevice.set(null);
      this.error.set('');
      this.newDevice.set(true);
    }
  }

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
      next: () => {
        this.saving.set(false);
        this.closeForm();
        this.loadDevices();
      },
      error: (e) => {
        this.saving.set(false);
        this.error.set(e.error?.detail || 'Failed to save.');
      },
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
      next: () => {
        this.saving.set(false);
        this.pairStep.set('done');
        this.loadDevices();
      },
      error: (e) => {
        this.saving.set(false);
        this.error.set(e.error?.detail || 'PIN verification failed.');
      },
    });
  }

  unpair(d: any) {
    if (confirm(`Unpair "${d.name}"?`)) {
      this.api.resetPairing(d.device_id).subscribe({ next: () => this.loadDevices() });
    }
  }

  closePair() {
    this.showPairModal.set(false);
    this.pairDevice.set(null);
  }

  openAddFromDiscovery(d: any) {
    this.form = { device_id: '', name: d.hostname || '', ip_address: d.ip_address };
    this.editDevice.set(null);
    this.error.set('');
    this.newDevice.set(false);
    this.showForm.set(true);
  }

  dismissNewDevice() {}

  closeDiscovery() {
    this.newDevice.set(false);
  }
  onOverlay(e: MouseEvent) {
    if ((e.target as HTMLElement).classList.contains('modal-overlay')) {
      this.closeForm();
      this.closePair();
    }
  }
}
