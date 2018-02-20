import { Injectable } from '@angular/core';
import {CanActivate, Router, RouterStateSnapshot, ActivatedRouteSnapshot} from '@angular/router';

import { ZilliqaService } from './zilliqa.service';

@Injectable()
export class AuthGuardService implements CanActivate {

  constructor(private router: Router, private zilliqaService: ZilliqaService) {}

  canActivate(route: ActivatedRouteSnapshot, state: RouterStateSnapshot): boolean {
    // check if user wallet is present
    const isLoggedIn = (this.zilliqaService.getWallet().address != undefined)

    if (isLoggedIn) {
      return true;
    } else {
      this.router.navigate(['/home']);
      return false;
    }
  }
}
