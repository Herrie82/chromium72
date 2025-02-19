// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import {produce} from 'immer';

import {assertExists} from '../base/logging';
import {Remote} from '../base/remote';
import {DeferredAction, StateActions} from '../common/actions';
import {createEmptyState, State} from '../common/state';

import {ControllerAny} from './controller';
import {Engine} from './engine';
import {WasmEngineProxy} from './wasm_engine_proxy';
import {
  createWasmEngine,
  destroyWasmEngine,
} from './wasm_engine_proxy';


export interface App {
  state: State;
  dispatch(action: DeferredAction): void;
  publish(
      what: 'OverviewData'|'TrackData'|'Threads'|'QueryResult'|'LegacyTrace',
      data: {}, transferList?: Array<{}>): void;
}

/**
 * Global accessors for state/dispatch in the controller.
 */
class Globals implements App {
  private _state?: State;
  private _rootController?: ControllerAny;
  private _frontend?: Remote;
  private _runningControllers = false;
  private _queuedActions = new Array<DeferredAction>();

  initialize(rootController: ControllerAny, frontendProxy: Remote) {
    this._rootController = rootController;
    this._frontend = frontendProxy;
    this._state = createEmptyState();
  }

  dispatch(action: DeferredAction): void {
    this.dispatchMultiple([action]);
  }

  dispatchMultiple(actions: DeferredAction[]): void {
    this._queuedActions = this._queuedActions.concat(actions);

    // If we are in the middle of running the controllers, queue the actions
    // and run them at the end of the run, so the state is atomically updated
    // only at the end and all controllers see the same state.
    if (this._runningControllers) return;

    this.runControllers();
  }

  private runControllers() {
    if (this._runningControllers) throw new Error('Re-entrant call detected');

    // Run controllers locally until all state machines reach quiescence.
    let runAgain = false;
    let summary = this._queuedActions.map(action => action.type).join(', ');
    summary = `Controllers loop (${summary})`;
    for (let iter = 0; runAgain || this._queuedActions.length > 0; iter++) {
      if (iter > 100) throw new Error('Controllers are stuck in a livelock');
      const actions = this._queuedActions;
      this._queuedActions = new Array<DeferredAction>();
      for (const action of actions) {
        this.applyAction(action);
      }
      this._runningControllers = true;
      try {
        runAgain = assertExists(this._rootController).invoke();
      } finally {
        this._runningControllers = false;
      }
    }
    assertExists(this._frontend).send<void>('updateState', [this.state]);
  }

  createEngine(): Engine {
    const id = new Date().toUTCString();
    const portAndId = {id, worker: createWasmEngine(id)};
    return new WasmEngineProxy(portAndId);
  }

  destroyEngine(id: string): void {
    destroyWasmEngine(id);
  }

  // TODO: this needs to be cleaned up.
  publish(
      what: 'OverviewData'|'TrackData'|'Threads'|'QueryResult'|'LegacyTrace',
      data: {}, transferList?: Array<{}>) {
    assertExists(this._frontend)
        .send<void>(`publish${what}`, [data], transferList);
  }

  get state(): State {
    return assertExists(this._state);
  }

  applyAction(action: DeferredAction): void {
    assertExists(this._state);
    // We need a special case for when we want to replace the whole tree.
    if (action.type === 'setState') {
      const args = (action as DeferredAction<{newState: State}>).args;
      this._state = args.newState;
      return;
    }
    // 'produce' creates a immer proxy which wraps the current state turning
    // all imperative mutations of the state done in the callback into
    // immutable changes to the returned state.
    this._state = produce(this.state, draft => {
      // tslint:disable-next-line no-any
      (StateActions as any)[action.type](draft, action.args);
    });
  }

  resetForTesting() {
    this._state = undefined;
    this._rootController = undefined;
  }
}

export const globals = new Globals();
