import { Injectable } from '@angular/core';
import { Observable } from 'rxjs/Observable';
import { of } from 'rxjs/observable/of';
import { catchError, map, tap } from 'rxjs/operators';
import { HttpClient, HttpHeaders } from '@angular/common/http';

import $ from 'jquery';
import { Wallet } from './wallet/wallet';
import { zLib } from 'z-lib';
import {secp256k1, hash256} from 'bcrypto';
import sha3 from 'bcrypto/lib/sha3';
import schnorr from 'schnorr';


@Injectable()
export class ZilliqaService {

  zlib: any;
  node: any;
  walletData: {};
  nodeData: {};
  userWallet: Wallet

  constructor(private http: HttpClient) {
    this.userWallet = new Wallet()
    this.walletData = {
      version: null
    };
    this.nodeData = {
      networkId: null,
      latestDsBlock: null
    };
    this.initLib();
  }

  initLib() {
    this.zlib = new zLib({
      nodeUrl: 'http://localhost:4201'
    });
    this.node = this.zlib.getNode();
  }

  getInitData() {
    var deferred = new $.Deferred();
    var that = this;

    that.node.getNetworkId(function(err, data1) {
      if (err) deferred.reject(err);

      that.node.getLatestDsBlock(function(err, data2) {
        if (err) deferred.reject(err);

        deferred.resolve({
          networkId: data1.result,
          latestDSBlock: data2.result
        });
      });
    });

    return deferred.promise();
  }

  getWallet() {
    return this.userWallet
  }

  generateWalletJson(passphrase) {

  }

  getBalance(address) {
    var deferred = new $.Deferred()

    this.node.getBalance({address: address}, function(err, data) {
      if (err) deferred.reject(err)

      deferred.resolve({
        address: address,
        balance: data.result
      })
    })

    return deferred.promise()
  }

  createWallet() {
    let key = secp256k1.generatePrivateKey()
    let pub = secp256k1.publicKeyCreate(key, true)

    let publicKeyHash = sha3.digest(pub) // sha3 hash of the public key
    let address = publicKeyHash.toString('hex', 12) // rightmost 160 bits/20 bytes of the hash

    this.userWallet = {
      address: address,
      balance: 0
    }

    return {privateKey: key.toString('hex')}  // don't store private key
  }

  importWallet(privateKey) {
    // checkValid(privateKey)
    this.userWallet = {
      address: 'abc',
      balance: 0
    }
    // if private key valid, return true else return false
    return true
  }

  parseWallet(uploadedWallet: File) {

  }

  resetWallet() {
    this.userWallet = new Wallet()
  }

  sendPayment(payment) {
    // checkValid(payment.address)
    var deferred = new $.Deferred()

    let tx = {
      nonce: 0,
      to: payment.address,
      amount: payment.amount,
      pubKey: '',
      gasPrice: payment.gasPrice,
      gasLimit: payment.gasLimit
    }

    this.node.createTransaction(tx, function(err, data) {
      if (err) deferred.reject(err)

      deferred.resolve({
        txId: data.result
      })
    })

    return deferred.promise()
  }

  getTxHistory() {
    var deferred = new $.Deferred()

    this.node.getTransactionHistory({address: this.userWallet.address}, function(err, data) {
      if (err) deferred.reject(err)

      // deferred.resolve(data.result)
      deferred.resolve(
        [{
          id: '0x123',
          to: '0x456',
          from: '0x789',
          amount: 59
        },
        {
          id: '0xeee',
          to: '0xfff',
          from: '0xddd',
          amount: 100
        },
        {
          id: '0x111212312312312123132fgrebgr34',
          to: '0x111212312312312123132234245433',
          from: '0x2768r73gireh32r8y734g9ure3y28rih',
          amount: 18
        },
        {
          id: '0x2ihu3gr398gi3g234g90243gijubeh39fio',
          to: '0x2j49f84jg983jg9384jg938gh398g398hg394gh3948g3',
          from: '0x892j4g398jg34g2398j432f983o9gj398gj3gh4g8',
          amount: 32
        }]
      )
    })

    return deferred.promise()
  }

  /**
   * Handle Http operation that failed - let the app continue
   * @param operation - name of the operation that failed
   * @param result - optional value to return as the observable result
   */
  private handleError<T> (operation = 'operation', result?: T) {
    return (error: any): Observable<T> => {

      console.error(error); // log to console

      // TODO: better job of transforming error for user consumption
      // this.log(`${operation} failed: ${error.message}`);

      // Let the app keep running by returning an empty result.
      return of(result as T);
    };
  }

  // searchHeroes(term: string): Observable<Hero[]> {
  //   if (!term.trim()) {
  //     // return empty array in case of blank search term
  //     return of([]);
  //   }
  //   return this.http.get<Hero[]>(`api/heroes/name=${term}`)
  //     .pipe(
  //       catchError(this.handleError<Hero[]>('searchHeroes', []))
  //     );
  // }

  //  getHeroes(): Observable<Hero[]> {
  //   return this.http.get<Hero[]>(this.heroesUrl)
  //     .pipe(
  //       catchError(this.handleError('getHeroes', []));
  //     );
  // }

  // getHero(id: number): Observable<Hero> {
  //   const url = `${this.heroesUrl}/${id}`;

  //   return this.http.get<Hero>(url)
  //     .pipe(
  //       catchError(this.handleError<Hero>(`getHero id=${id}`));
  //     );
  // }

  // updateHero(id: number): Observable<any> {
  //   return this.http.put(this.heroesUrl, hero, httpOptions)
  //     .pipe(
  //       catchError(this.handleError<any>('updateHero'))
  //     );
  // }
}
