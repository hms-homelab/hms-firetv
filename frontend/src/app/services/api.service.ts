import { Injectable, inject } from '@angular/core';
import { HttpClient } from '@angular/common/http';

@Injectable({ providedIn: 'root' })
export class ApiService {
  private http = inject(HttpClient);

  getHealth() { return this.http.get<any>('/health'); }
  getStatus() { return this.http.get<any>('/status'); }

  getDevices() { return this.http.get<any>('/api/devices'); }
  getDevice(id: string) { return this.http.get<any>(`/api/devices/${id}`); }
  createDevice(device: any) { return this.http.post<any>('/api/devices', device); }
  updateDevice(id: string, device: any) { return this.http.put<any>(`/api/devices/${id}`, device); }
  deleteDevice(id: string) { return this.http.delete<any>(`/api/devices/${id}`); }

  startPairing(id: string) { return this.http.post<any>(`/api/devices/${id}/pair/start`, {}); }
  verifyPairing(id: string, pin: string) { return this.http.post<any>(`/api/devices/${id}/pair/verify`, { pin }); }
  resetPairing(id: string) { return this.http.post<any>(`/api/devices/${id}/pair/reset`, {}); }
  getDeviceStatus(id: string) { return this.http.get<any>(`/api/devices/${id}/status`); }

  sendNavigation(id: string, action: string) { return this.http.post<any>(`/api/devices/${id}/navigate`, { action }); }
  sendMedia(id: string, action: string) { return this.http.post<any>(`/api/devices/${id}/media`, { action }); }
  sendKeyboard(id: string, text: string) { return this.http.post<any>(`/api/devices/${id}/text`, { text }); }
  launchApp(id: string, pkg: string) { return this.http.post<any>(`/api/devices/${id}/app`, { package: pkg }); }

  getApps(id: string) { return this.http.get<any>(`/api/devices/${id}/apps`); }
  addApp(id: string, app: any) { return this.http.post<any>(`/api/devices/${id}/apps`, app); }
  deleteApp(id: string, pkg: string) { return this.http.delete<any>(`/api/devices/${id}/apps/${encodeURIComponent(pkg)}`); }
  getPopularApps(category?: string) { return this.http.get<any>(`/api/apps/popular${category ? '?category=' + category : ''}`); }

  getStats() { return this.http.get<any>('/api/stats'); }
}
