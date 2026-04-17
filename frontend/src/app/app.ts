import { Component } from '@angular/core';
import { RouterOutlet } from '@angular/router';
import { NavBarComponent } from './components/nav-bar/nav-bar.component';

@Component({
  selector: 'app-root',
  standalone: true,
  imports: [RouterOutlet, NavBarComponent],
  template: `
    <app-nav-bar />
    <main>
      <router-outlet />
    </main>
  `,
  styles: [`
    :host { display: block; min-height: 100vh; background: #121212; }
    main { max-width: 1200px; margin: 0 auto; }
  `],
})
export class App {}
