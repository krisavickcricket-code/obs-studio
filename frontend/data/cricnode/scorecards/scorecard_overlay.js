/**
 * v252: Shared scorecard overlay renderer.
 *
 * Injected into Play-Cricket / PlayHQ pages BEFORE the platform bridge.
 * Creates a full-screen overlay div (z-index:999999) with a DCL-style
 * cricket scorecard, hiding the raw website underneath.
 *
 * Usage from bridge JS:
 *   CN.init({ accent:'#800020', accentLight:'#A0334F', accentDark:'#5C0018',
 *             badgeLabel:'Play-Cricket', provider:'playcricket' });
 *   CN.update(normalizedData);
 *
 * Normalized data format accepted by CN.update():
 *   {
 *     teamA, teamB,
 *     activeInnings: 1|2,
 *     score: "145/6", overs: "23.4",
 *     runRate: "6.17",
 *     target: null,
 *     requiredRuns: null,
 *     striker:    { name, runs, balls, fours, sixes, sr },
 *     nonStriker: { name, runs, balls, fours, sixes },
 *     bowler:     { name, overs, maidens, runs, wickets, economy },
 *     lastWicket: { name, runs, balls, howOut },
 *     recentBalls: [],
 *     isMatchEnded: false,
 *     resultMessage: null,
 *     firstInningsScore: null
 *   }
 */
(function() {
    'use strict';
    if (window.CN) return;

    var TAG = '[CN Overlay]';
    function log(msg) { try { console.log(TAG + ' ' + msg); } catch(e) {} }

    // ========== STATE ==========
    var _initialized = false;
    var _config = {
        accent: '#1B5E20',
        accentLight: '#2E7D32',
        accentDark: '#0D3B12',
        badgeLabel: '',
        provider: ''
    };
    var _feat = { intro: true, player: true, wicket: true, innings: true, sidepanel: true, result: true };
    var _lastData = null;
    var _lastScore = '';
    var _lastWickets = -1;
    var _lastOvers = '';
    var _lastInnings = 0;
    var _lastStateKey = '';
    var _introShown = false;
    var _innBreakShown = false;
    var _resultShown = false;
    var _prevStriker = '';
    var _prevNonStriker = '';
    var _prevBowler = '';
    var _lastBallCnt = 0;

    // ========== CSS INJECTION ==========
    function injectCSS() {
        if (document.getElementById('cn-overlay-css')) return;
        var style = document.createElement('style');
        style.id = 'cn-overlay-css';
        style.textContent = [
            '#cn-root {',
            '  position: fixed; top: 0; left: 0; right: 0; bottom: 0;',
            '  z-index: 999999; pointer-events: none;',
            '  font-family: "Segoe UI", -apple-system, BlinkMacSystemFont, Roboto, sans-serif;',
            '  overflow: hidden;',
            '}',
            '#cn-root * { margin: 0; padding: 0; box-sizing: border-box; }',
            '',
            ':root {',
            '  --cn-base: clamp(8px, 1.15vw, 22px);',
            '  --cn-score-font: clamp(13px, 2vw, 38px);',
            '  --cn-ball-size: clamp(10px, 1.4vw, 26px);',
            '  --cn-ball-font: clamp(5px, 0.7vw, 13px);',
            '  --cn-h: clamp(32px, 4.2vw, 80px);',
            '  --cn-info-h: clamp(14px, 1.7vw, 32px);',
            '  --cn-radius: clamp(3px, 0.4vw, 8px);',
            '  --cn-accent: ' + _config.accent + ';',
            '  --cn-accent-light: ' + _config.accentLight + ';',
            '  --cn-accent-dark: ' + _config.accentDark + ';',
            '}',
            '',
            /* Badge */
            '#cn-badge {',
            '  position: fixed; top: clamp(6px,0.6vw,10px); right: clamp(8px,0.8vw,14px);',
            '  z-index: 1000010; pointer-events: none;',
            '  opacity: 0; transition: opacity 0.4s;',
            '  background: rgba(255,255,255,0.95); border-radius: 50%;',
            '  width: clamp(38px,4.5vw,86px); height: clamp(38px,4.5vw,86px);',
            '  display: flex; align-items: center; justify-content: center;',
            '  box-shadow: 0 1px 6px rgba(0,0,0,0.25);',
            '  font-size: clamp(5px,0.55vw,10px); font-weight: 700;',
            '  text-transform: uppercase; letter-spacing: 0.5px;',
            '  color: var(--cn-accent-dark); text-align: center;',
            '}',
            '#cn-badge.vis { opacity: 1; }',
            '',
            /* Overlay wrapper */
            '#cn-overlay {',
            '  position: fixed; bottom: 0; left: 0; right: 0;',
            '  z-index: 1000000; pointer-events: none;',
            '  opacity: 0; transition: opacity 0.4s;',
            '  padding: 0 clamp(16px,2vw,40px) clamp(8px,0.8vw,16px);',
            '}',
            '#cn-overlay.vis { opacity: 1; }',
            '',
            /* Wait state */
            '#cn-wait {',
            '  display: flex; flex-direction: column; align-items: center;',
            '  justify-content: center; background: rgba(255,255,255,0.97);',
            '  padding: clamp(6px,0.6vw,10px) clamp(10px,1vw,16px);',
            '  text-align: center; border-radius: var(--cn-radius);',
            '  box-shadow: 0 1px 8px rgba(0,0,0,0.12); color: #333;',
            '}',
            '#cn-wait.hide { display: none; }',
            '.cn-w-title { font-size: clamp(10px,0.85vw,13px); font-weight: 600; color: #222; }',
            '.cn-w-msg { font-size: calc(var(--cn-base)*0.85); color: #999; margin-top: 2px; }',
            '',
            /* Main bar */
            '#cn-live { display: none; }',
            '#cn-live.vis { display: block; }',
            '.cn-bar {',
            '  display: flex; align-items: stretch; height: var(--cn-h);',
            '  background: rgba(255,255,255,0.97);',
            '  border-radius: var(--cn-radius) var(--cn-radius) 0 0;',
            '  box-shadow: 0 -1px 8px rgba(0,0,0,0.12); overflow: hidden;',
            '}',
            '.cn-cell { display: flex; align-items: center; }',
            '',
            /* Team boxes */
            '.cn-team-box {',
            '  display: flex; align-items: center; justify-content: center;',
            '  padding: 0 clamp(8px,0.8vw,14px); background: var(--cn-accent);',
            '  color: #fff; font-size: calc(var(--cn-base)*0.9);',
            '  font-weight: 600; letter-spacing: 0.5px; text-transform: uppercase;',
            '  white-space: nowrap; min-width: clamp(60px,8vw,160px);',
            '}',
            '.cn-team-box.cn-bowl-box { background: #37474F; }',
            '',
            /* Spacer */
            '.cn-spacer { flex: 1; min-width: clamp(4px,0.5vw,10px); }',
            '',
            /* Batsmen */
            '.cn-bat {',
            '  display: flex; flex-direction: column; justify-content: center;',
            '  align-items: flex-end; padding: 0 clamp(5px,0.5vw,8px); flex-shrink: 0;',
            '}',
            '.cn-b-row {',
            '  display: flex; align-items: baseline; gap: clamp(3px,0.3vw,5px);',
            '  line-height: 1.3; white-space: nowrap;',
            '}',
            '.cn-b-name {',
            '  font-size: var(--cn-base); font-weight: 400; color: #555;',
            '  overflow: hidden; text-overflow: ellipsis; text-align: right;',
            '  max-width: clamp(80px,14vw,260px);',
            '}',
            '.cn-b-name.cn-strike { color: #111; font-weight: 700; }',
            '.cn-b-fig { font-size: calc(var(--cn-base)*0.85); color: #aaa; flex-shrink: 0; }',
            '.cn-b-fig b { color: #333; font-weight: 600; }',
            '',
            /* Score center */
            '.cn-score { display: flex; align-items: stretch; flex-shrink: 0; }',
            '.cn-score-left {',
            '  display: flex; flex-direction: column; justify-content: center;',
            '  align-items: center; background: var(--cn-accent-light); color: #fff;',
            '  padding: 0 clamp(6px,0.6vw,10px); min-width: clamp(28px,2.8vw,44px);',
            '}',
            '.cn-score-left .cn-abbr { font-size: calc(var(--cn-base)*0.85); font-weight: 700; letter-spacing: 0.5px; }',
            '.cn-score-left .cn-rr { font-size: calc(var(--cn-base)*0.6); opacity: 0.8; }',
            '.cn-score-right {',
            '  display: flex; flex-direction: column; justify-content: center;',
            '  align-items: center; background: var(--cn-accent-dark); color: #fff;',
            '  padding: 0 clamp(8px,0.8vw,14px); min-width: clamp(40px,4vw,64px);',
            '}',
            '.cn-score-right .cn-sc {',
            '  font-size: var(--cn-score-font); font-weight: 700;',
            '  line-height: 1.1; transition: transform 0.2s;',
            '}',
            '.cn-score-right .cn-sc.cn-flash { transform: scale(1.08); }',
            '.cn-score-right .cn-ov-sm { font-size: calc(var(--cn-base)*0.6); opacity: 0.7; }',
            '',
            /* Bowler */
            '.cn-bowl {',
            '  display: flex; flex-direction: column; justify-content: center;',
            '  padding: 0 clamp(5px,0.5vw,8px); flex-shrink: 0;',
            '  border-left: 1px solid #e8e8e8;',
            '}',
            '.cn-bw-name {',
            '  font-size: var(--cn-base); font-weight: 400; color: #555;',
            '  white-space: nowrap; overflow: hidden; text-overflow: ellipsis;',
            '  max-width: clamp(80px,14vw,260px);',
            '}',
            '.cn-bw-fig { font-size: calc(var(--cn-base)*0.85); color: #aaa; }',
            '.cn-bw-fig .cn-wk { color: #C62828; font-weight: 600; }',
            '',
            /* Overs */
            '.cn-overs {',
            '  display: flex; flex-direction: column; justify-content: center;',
            '  align-items: center; padding: 0 clamp(4px,0.4vw,6px);',
            '  flex-shrink: 0; border-left: 1px solid #e8e8e8;',
            '}',
            '.cn-ov-label {',
            '  font-size: calc(var(--cn-base)*0.55); color: #bbb;',
            '  text-transform: uppercase; letter-spacing: 0.5px; font-weight: 600;',
            '}',
            '.cn-ov-val { font-size: calc(var(--cn-base)*1.1); font-weight: 700; color: #222; line-height: 1.2; }',
            '',
            /* Ball dots */
            '.cn-balls {',
            '  display: flex; flex-wrap: wrap; justify-content: flex-start;',
            '  align-content: center; gap: clamp(1.5px,0.15vw,2.5px);',
            '  padding: 0 clamp(4px,0.4vw,6px); flex-shrink: 0;',
            '  border-left: 1px solid #e8e8e8;',
            '}',
            '.cn-dot {',
            '  width: var(--cn-ball-size); height: var(--cn-ball-size);',
            '  border-radius: 50%; display: flex; align-items: center;',
            '  justify-content: center; font-size: var(--cn-ball-font);',
            '  font-weight: 700; color: #fff;',
            '}',
            '.cn-dot.cn-pop { animation: cnPop 0.25s ease-out; }',
            '@keyframes cnPop { 0%{transform:scale(0)} 70%{transform:scale(1.15)} 100%{transform:scale(1)} }',
            '',
            /* Info strip */
            '.cn-info {',
            '  display: flex; height: var(--cn-info-h);',
            '  font-size: calc(var(--cn-base)*0.78); font-weight: 500;',
            '  letter-spacing: 0.3px; color: #fff;',
            '  border-radius: 0 0 var(--cn-radius) var(--cn-radius); overflow: hidden;',
            '}',
            '.cn-info-main {',
            '  flex: 1; display: flex; align-items: center;',
            '  padding: 0 clamp(8px,0.8vw,14px); background: #212121;',
            '  white-space: nowrap; overflow: hidden; gap: clamp(6px,0.7vw,12px);',
            '}',
            '.cn-info-main .cn-teams { text-transform: uppercase; letter-spacing: 0.6px; opacity: 0.85; }',
            '.cn-info-main .cn-chase-txt { color: #81C784; }',
            '.cn-info-main .cn-result-txt { color: #81C784; font-weight: 600; }',
            '.cn-info-main .cn-inn1-pill {',
            '  background: rgba(255,255,255,0.1); padding: 0 clamp(3px,0.3vw,5px);',
            '  border-radius: 2px; font-size: calc(var(--cn-base)*0.72);',
            '}',
            '.cn-info-brand {',
            '  display: flex; align-items: center; gap: clamp(3px,0.3vw,5px);',
            '  padding: 0 clamp(8px,0.8vw,14px); background: #212121;',
            '  color: rgba(255,255,255,0.45); font-size: calc(var(--cn-base)*0.68);',
            '  white-space: nowrap; letter-spacing: 0.5px;',
            '  border-left: 1px solid rgba(255,255,255,0.08);',
            '}',
            '',
            /* Side panel */
            '#cn-side-panel {',
            '  position: fixed;',
            '  right: clamp(16px,2vw,40px);',
            '  bottom: calc(var(--cn-h) + var(--cn-info-h) + clamp(16px,1.6vw,32px));',
            '  z-index: 1000005; pointer-events: none;',
            '  width: clamp(160px,18vw,340px);',
            '  background: rgba(0,0,0,0.82); border-radius: var(--cn-radius);',
            '  color: #fff; font-size: calc(var(--cn-base)*0.85);',
            '  opacity: 0; transform: translateX(20px);',
            '  transition: opacity 0.4s, transform 0.4s; overflow: hidden;',
            '}',
            '#cn-side-panel.vis { opacity: 1; transform: translateX(0); }',
            '.cn-sp-section { padding: clamp(4px,0.4vw,8px) clamp(6px,0.6vw,10px); }',
            '.cn-sp-section + .cn-sp-section { border-top: 1px solid rgba(255,255,255,0.1); }',
            '.cn-sp-title {',
            '  font-size: calc(var(--cn-base)*0.65); font-weight: 700;',
            '  text-transform: uppercase; letter-spacing: 0.8px;',
            '  color: rgba(255,255,255,0.5); margin-bottom: 2px;',
            '}',
            '.cn-sp-main { font-weight: 600; font-size: calc(var(--cn-base)*0.95); }',
            '.cn-sp-detail { font-size: calc(var(--cn-base)*0.75); color: rgba(255,255,255,0.7); line-height: 1.4; }',
            '',
            /* Card overlays */
            '.cn-card-overlay {',
            '  position: fixed; top: 0; left: 0; right: 0; bottom: 0;',
            '  z-index: 1000020; display: none; pointer-events: none;',
            '  align-items: center; justify-content: center;',
            '  background: rgba(0,0,0,0.6);',
            '}',
            '.cn-card-overlay.vis { display: flex; }',
            '.cn-card {',
            '  background: rgba(20,20,20,0.95); border-radius: clamp(6px,0.6vw,12px);',
            '  padding: clamp(16px,2vw,40px); color: #fff; text-align: center;',
            '  min-width: clamp(260px,30vw,550px); max-width: 60vw;',
            '  box-shadow: 0 4px 30px rgba(0,0,0,0.5); animation: cnCardIn 0.4s ease-out;',
            '}',
            '@keyframes cnCardIn { 0%{transform:scale(0.9);opacity:0} 100%{transform:scale(1);opacity:1} }',
            '.cn-card-title {',
            '  font-size: clamp(10px,1.2vw,22px); font-weight: 700;',
            '  text-transform: uppercase; letter-spacing: 1.5px;',
            '  margin-bottom: clamp(8px,0.8vw,16px);',
            '}',
            '.cn-card-divider { height: 1px; background: rgba(255,255,255,0.15); margin: clamp(6px,0.6vw,12px) 0; }',
            '.cn-card-detail { font-size: calc(var(--cn-base)*0.85); color: rgba(255,255,255,0.7); line-height: 1.5; }',
            '.cn-card-result {',
            '  font-size: clamp(10px,1.1vw,20px); font-weight: 700;',
            '  color: #81C784; margin-top: clamp(4px,0.4vw,8px);',
            '}',
            '.cn-card-brand {',
            '  margin-top: clamp(10px,1vw,18px); font-size: calc(var(--cn-base)*0.65);',
            '  color: rgba(255,255,255,0.35); letter-spacing: 0.5px;',
            '  display: flex; align-items: center; justify-content: center;',
            '}',
            '.cn-card-team-row {',
            '  display: flex; align-items: center; justify-content: center;',
            '  gap: clamp(8px,0.8vw,14px); margin: clamp(4px,0.4vw,8px) 0;',
            '}',
            '.cn-card-team-name { font-size: clamp(10px,1.3vw,24px); font-weight: 600; }',
            '.cn-card-vs {',
            '  font-size: clamp(8px,0.9vw,16px); font-weight: 700;',
            '  color: rgba(255,255,255,0.5); letter-spacing: 1px;',
            '}',
            '.cn-card-score { font-size: clamp(14px,1.8vw,34px); font-weight: 700; }',
            '.cn-card-overs { font-size: calc(var(--cn-base)*0.75); color: rgba(255,255,255,0.5); }',
            '.cn-card.cn-wicket .cn-card-title { color: #EF5350; }',
            '.cn-card-player-name { font-size: clamp(10px,1.2vw,22px); font-weight: 600; }',
            '.cn-card-player-score { font-size: clamp(12px,1.5vw,28px); font-weight: 700; }',
            '.cn-card-player-detail { font-size: calc(var(--cn-base)*0.8); color: rgba(255,255,255,0.6); }',
            ''
        ].join('\n');
        document.head.appendChild(style);
    }

    // ========== HTML INJECTION ==========
    function injectHTML() {
        if (document.getElementById('cn-root')) return;
        var root = document.createElement('div');
        root.id = 'cn-root';
        root.innerHTML = [
            '<div id="cn-badge">' + (_config.badgeLabel || '') + '</div>',
            '',
            '<div id="cn-overlay">',
            '  <div id="cn-wait">',
            '    <div class="cn-w-title" id="cn-wT">Loading match...</div>',
            '    <div class="cn-w-msg" id="cn-wM">Connecting...</div>',
            '  </div>',
            '',
            '  <div id="cn-live">',
            '    <div class="cn-bar">',
            '      <div class="cn-team-box" id="cn-batTeamBox">BATTING</div>',
            '      <div class="cn-spacer"></div>',
            '      <div class="cn-bat">',
            '        <div class="cn-b-row">',
            '          <span class="cn-b-name cn-strike" id="cn-striker">&mdash;*</span>',
            '          <span class="cn-b-fig"><b id="cn-sR">0</b> <span id="cn-sB">(0)</span></span>',
            '        </div>',
            '        <div class="cn-b-row">',
            '          <span class="cn-b-name" id="cn-nonStriker">&mdash;</span>',
            '          <span class="cn-b-fig"><b id="cn-nsR">0</b> <span id="cn-nsB">(0)</span></span>',
            '        </div>',
            '      </div>',
            '      <div class="cn-score">',
            '        <div class="cn-score-left">',
            '          <span class="cn-abbr" id="cn-batAbbr">---</span>',
            '          <span class="cn-rr" id="cn-rr">RR 0.00</span>',
            '        </div>',
            '        <div class="cn-score-right">',
            '          <span class="cn-sc" id="cn-scoreMain">0/0</span>',
            '          <span class="cn-ov-sm" id="cn-ovSmall">0.0 ov</span>',
            '        </div>',
            '      </div>',
            '      <div class="cn-bowl">',
            '        <span class="cn-bw-name" id="cn-bowlName">&mdash;</span>',
            '        <span class="cn-bw-fig"><span class="cn-wk" id="cn-bowlW">0</span>/<span id="cn-bowlR">0</span> <span id="cn-bowlOv" style="color:#aaa"></span></span>',
            '      </div>',
            '      <div class="cn-overs">',
            '        <span class="cn-ov-label">OVERS</span>',
            '        <span class="cn-ov-val" id="cn-overs">0.0</span>',
            '      </div>',
            '      <div class="cn-cell cn-balls" id="cn-balls"></div>',
            '      <div class="cn-spacer"></div>',
            '      <div class="cn-team-box cn-bowl-box" id="cn-bowlTeamBox">BOWLING</div>',
            '    </div>',
            '    <div class="cn-info">',
            '      <div class="cn-info-main" id="cn-infoMain">',
            '        <span class="cn-teams" id="cn-infoTeams">Team 1 &bull; Team 2</span>',
            '      </div>',
            '      <div class="cn-info-brand">POWERED BY CricNode</div>',
            '    </div>',
            '  </div>',
            '</div>',
            '',
            '<div id="cn-side-panel">',
            '  <div class="cn-sp-section">',
            '    <div class="cn-sp-title">RUN RATE</div>',
            '    <div class="cn-sp-main" id="cn-spRunRate">0.00</div>',
            '    <div class="cn-sp-detail" id="cn-spRRR"></div>',
            '  </div>',
            '  <div class="cn-sp-section" id="cn-spLastWicket" style="display:none;">',
            '    <div class="cn-sp-title">LAST WICKET</div>',
            '    <div class="cn-sp-main" id="cn-spLwName"></div>',
            '    <div class="cn-sp-detail" id="cn-spLwHow"></div>',
            '  </div>',
            '</div>',
            '',
            /* Intro card */
            '<div class="cn-card-overlay" id="cn-introOverlay">',
            '  <div class="cn-card">',
            '    <div class="cn-card-team-row"><div class="cn-card-team-name" id="cn-introT1"></div></div>',
            '    <div class="cn-card-vs">VS</div>',
            '    <div class="cn-card-team-row"><div class="cn-card-team-name" id="cn-introT2"></div></div>',
            '    <div class="cn-card-divider"></div>',
            '    <div class="cn-card-detail" id="cn-introDetail"></div>',
            '    <div class="cn-card-brand">POWERED BY CricNode</div>',
            '  </div>',
            '</div>',
            '',
            /* Wicket card */
            '<div class="cn-card-overlay" id="cn-wicketOverlay">',
            '  <div class="cn-card cn-wicket">',
            '    <div class="cn-card-title">WICKET!</div>',
            '    <div class="cn-card-player-name" id="cn-wkName"></div>',
            '    <div class="cn-card-player-score" id="cn-wkScore"></div>',
            '    <div class="cn-card-player-detail" id="cn-wkDetail"></div>',
            '    <div class="cn-card-divider"></div>',
            '    <div class="cn-card-detail" id="cn-wkHow"></div>',
            '  </div>',
            '</div>',
            '',
            /* Innings break card */
            '<div class="cn-card-overlay" id="cn-inningsOverlay">',
            '  <div class="cn-card">',
            '    <div class="cn-card-title">INNINGS BREAK</div>',
            '    <div class="cn-card-divider"></div>',
            '    <div class="cn-card-team-row"><div><div class="cn-card-team-name" id="cn-innTeam"></div><div class="cn-card-score" id="cn-innScore"></div><div class="cn-card-overs" id="cn-innOvers"></div></div></div>',
            '    <div class="cn-card-divider"></div>',
            '    <div class="cn-card-detail" id="cn-innTarget"></div>',
            '    <div class="cn-card-brand">POWERED BY CricNode</div>',
            '  </div>',
            '</div>',
            '',
            /* Result card */
            '<div class="cn-card-overlay" id="cn-resultOverlay">',
            '  <div class="cn-card">',
            '    <div class="cn-card-title">MATCH RESULT</div>',
            '    <div class="cn-card-divider"></div>',
            '    <div class="cn-card-team-row"><div><div class="cn-card-team-name" id="cn-resT1"></div><div class="cn-card-score" id="cn-resS1"></div></div></div>',
            '    <div class="cn-card-team-row" style="margin-top:4px;"><div><div class="cn-card-team-name" id="cn-resT2"></div><div class="cn-card-score" id="cn-resS2"></div></div></div>',
            '    <div class="cn-card-divider"></div>',
            '    <div class="cn-card-result" id="cn-resResult"></div>',
            '    <div class="cn-card-brand">POWERED BY CricNode</div>',
            '  </div>',
            '</div>'
        ].join('\n');
        document.body.appendChild(root);
    }

    // ========== HELPERS ==========
    function txt(id, v) {
        var e = document.getElementById(id);
        if (e && e.textContent !== v) e.textContent = v;
    }
    function abbr(n) { return n ? n.substring(0, 3).toUpperCase() : '---'; }

    function showCard(id, duration) {
        var el = document.getElementById(id);
        if (!el) return;
        el.classList.add('vis');
        if (duration) setTimeout(function() { el.classList.remove('vis'); }, duration);
    }

    function ballBg(v) {
        var s = String(v);
        if (s === '0') return '#9E9E9E';
        if (s === '1' || s === '2' || s === '3') return '#43A047';
        if (s === '4') return '#00897B';
        if (s === '6') return '#E91E63';
        if (s === 'W' || s === 'Wt') return '#C62828';
        if (s === 'wd') return '#EC407A';
        if (s.indexOf('nb') === 0) return '#FF9800';
        if (s.indexOf('b') === 0) return '#78909C';
        return '#757575';
    }
    function ballTxt(v) {
        var s = String(v);
        if (s === 'wd') return 'WD';
        if (s === 'Wt') return 'W';
        if (s.indexOf('nb') === 0) return 'NB';
        if (s.indexOf('b') === 0) return 'B';
        return s;
    }

    function renderBalls(stack) {
        var c = document.getElementById('cn-balls');
        if (!c) return;
        if (!stack || !stack.length) { c.innerHTML = ''; _lastBallCnt = 0; return; }
        var r = stack.slice(-10);
        var key = r.join(',');
        if (c.dataset.k === key) return;
        var prev = _lastBallCnt;
        _lastBallCnt = r.length;
        c.innerHTML = '';
        r.forEach(function(v, i) {
            var el = document.createElement('span');
            el.className = 'cn-dot' + (i >= prev && prev > 0 ? ' cn-pop' : '');
            el.style.background = ballBg(v);
            el.textContent = ballTxt(v);
            c.appendChild(el);
        });
        c.dataset.k = key;
    }

    // ========== BRIDGE EVENTS ==========
    function fire(type, extra) {
        if (!window.CricNodeBridge) return;
        var d = _lastData || {};
        var evt = {
            type: type,
            timestamp: Date.now(),
            innings: d.activeInnings || 1,
            overs: d.overs || '',
            score: d.score || '',
            details: extra || {}
        };
        try {
            CricNodeBridge.onMatchEvent(JSON.stringify(evt));
            log('Fired: ' + type + ' score=' + (d.score || ''));
        } catch(e) { log('Error firing ' + type + ': ' + e); }
    }

    function buildStateDetails(d) {
        var details = {};
        if (d.striker) {
            details.striker = d.striker.name || '';
            details.strikerRuns = String(d.striker.runs || 0);
            details.strikerBalls = String(d.striker.balls || 0);
        }
        if (d.nonStriker) {
            details.nonStriker = d.nonStriker.name || '';
            details.nonStrikerRuns = String(d.nonStriker.runs || 0);
            details.nonStrikerBalls = String(d.nonStriker.balls || 0);
        }
        if (d.bowler) {
            details.bowler = d.bowler.name || '';
            details.bowlerOvers = d.bowler.overs || '';
            details.bowlerRuns = String(d.bowler.runs || 0);
            details.bowlerWickets = String(d.bowler.wickets || 0);
        }
        details.battingTeam = d.teamA || '';
        details.bowlingTeam = d.teamB || '';
        details.runRate = d.runRate || '';
        if (d.target) details.target = d.target;
        if (d.requiredRuns) details.requiredRuns = d.requiredRuns;
        return details;
    }

    // ========== CN.init ==========
    function init(cfg) {
        if (_initialized) return;
        _initialized = true;

        if (cfg) {
            if (cfg.accent) _config.accent = cfg.accent;
            if (cfg.accentLight) _config.accentLight = cfg.accentLight;
            if (cfg.accentDark) _config.accentDark = cfg.accentDark;
            if (cfg.badgeLabel) _config.badgeLabel = cfg.badgeLabel;
            if (cfg.provider) _config.provider = cfg.provider;
        }

        injectCSS();

        // Wait for body to be available
        if (document.body) {
            injectHTML();
            finishInit();
        } else {
            document.addEventListener('DOMContentLoaded', function() {
                injectHTML();
                finishInit();
            });
        }

        log('Initialized with provider=' + _config.provider);
    }

    function finishInit() {
        // Show the overlay in wait state
        var ov = document.getElementById('cn-overlay');
        if (ov) ov.classList.add('vis');
        var badge = document.getElementById('cn-badge');
        if (badge) badge.classList.add('vis');
    }

    // ========== CN.setFlags ==========
    function setFlags(flags) {
        if (flags) {
            for (var k in flags) {
                if (flags.hasOwnProperty(k) && _feat.hasOwnProperty(k)) {
                    _feat[k] = !!flags[k];
                }
            }
        }
    }

    // ========== CN.update ==========
    function update(d) {
        if (!d) return;
        if (!_initialized) {
            log('update() called before init(), auto-initializing');
            init({});
        }

        // Wait for DOM
        if (!document.getElementById('cn-root')) {
            setTimeout(function() { update(d); }, 200);
            return;
        }

        _lastData = d;

        var score = d.score || '0/0';
        var overs = d.overs || '0.0';
        var activeInn = d.activeInnings || 1;

        // Detect wicket (compare wicket count from score string)
        var parts = score.split('/');
        var curRuns = parseInt(parts[0], 10) || 0;
        var curWickets = parseInt(parts[1], 10) || 0;

        // --- Score change detection ---
        if (_lastScore && _lastScore !== score) {
            // Flash animation
            var el = document.getElementById('cn-scoreMain');
            if (el) {
                el.classList.add('cn-flash');
                setTimeout(function() { el.classList.remove('cn-flash'); }, 250);
            }
            fire('SCORE_UPDATE', { previousScore: _lastScore });

            // Boundary detection
            var prevRuns = parseInt((_lastScore || '0/0').split('/')[0], 10) || 0;
            var diff = curRuns - prevRuns;
            if (diff === 4) fire('BOUNDARY_4', { striker: d.striker ? d.striker.name : '' });
            else if (diff === 6) fire('BOUNDARY_6', { striker: d.striker ? d.striker.name : '' });
        }

        // --- Wicket detection ---
        if (_lastWickets >= 0 && curWickets > _lastWickets && _feat.wicket) {
            fire('WICKET', {
                playerName: d.lastWicket ? d.lastWicket.name : '',
                runs: d.lastWicket ? String(d.lastWicket.runs || 0) : '',
                balls: d.lastWicket ? String(d.lastWicket.balls || 0) : '',
                dismissalType: d.lastWicket ? (d.lastWicket.howOut || '') : ''
            });
            // Show wicket card
            if (d.lastWicket) {
                txt('cn-wkName', d.lastWicket.name || 'Unknown');
                txt('cn-wkScore', (d.lastWicket.runs || 0) + ' (' + (d.lastWicket.balls || 0) + ')');
                txt('cn-wkDetail', '');
                txt('cn-wkHow', d.lastWicket.howOut || '');
                showCard('cn-wicketOverlay', 6000);
            }
        }

        // --- Innings break detection ---
        if (_feat.innings && activeInn === 2 && _lastInnings === 1 && _lastInnings > 0) {
            if (!_innBreakShown) {
                _innBreakShown = true;
                if (d.firstInningsScore) {
                    txt('cn-innTeam', d.teamB || 'Team');
                    txt('cn-innScore', d.firstInningsScore);
                    txt('cn-innOvers', '');
                    var tgt = d.target || '';
                    txt('cn-innTarget', tgt ? 'Target: ' + tgt : '');
                    showCard('cn-inningsOverlay', 10000);
                    fire('INNINGS_BREAK', { target: tgt, innings: '1' });
                }
            }
        }

        // --- Match result ---
        if (_feat.result && d.isMatchEnded && !_resultShown) {
            _resultShown = true;
            txt('cn-resT1', d.teamA || 'Team 1');
            txt('cn-resT2', d.teamB || 'Team 2');
            txt('cn-resS1', d.firstInningsScore || score);
            txt('cn-resS2', activeInn === 2 ? score : '');
            txt('cn-resResult', d.resultMessage || 'Match Complete');
            showCard('cn-resultOverlay');
            fire('MATCH_RESULT', { result: d.resultMessage || '' });
        }

        // --- Match intro ---
        if (_feat.intro && !_introShown && !d.isMatchEnded && d.teamA && d.teamB) {
            _introShown = true;
            txt('cn-introT1', d.teamA);
            txt('cn-introT2', d.teamB);
            txt('cn-introDetail', _config.badgeLabel || '');
            showCard('cn-introOverlay', 8000);
        }

        // --- Update state tracking ---
        _lastScore = score;
        _lastWickets = curWickets;
        _lastOvers = overs;
        _lastInnings = activeInn;

        // --- Update live bar ---
        var batTeam = d.teamA || 'BATTING';
        var bowlTeam = d.teamB || 'BOWLING';

        txt('cn-batTeamBox', batTeam);
        txt('cn-bowlTeamBox', bowlTeam);

        // Striker
        if (d.striker) {
            txt('cn-striker', (d.striker.name || '\u2014') + '*');
            txt('cn-sR', String(d.striker.runs || 0));
            txt('cn-sB', '(' + String(d.striker.balls || 0) + ')');
        }
        // Non-striker
        if (d.nonStriker) {
            txt('cn-nonStriker', d.nonStriker.name || '\u2014');
            txt('cn-nsR', String(d.nonStriker.runs || 0));
            txt('cn-nsB', '(' + String(d.nonStriker.balls || 0) + ')');
        }

        // Score center
        txt('cn-batAbbr', abbr(batTeam));
        txt('cn-rr', 'RR ' + (d.runRate || '0.00'));
        txt('cn-scoreMain', score);
        txt('cn-ovSmall', overs + ' ov');
        txt('cn-overs', overs);

        // Bowler
        if (d.bowler) {
            txt('cn-bowlName', d.bowler.name || '\u2014');
            txt('cn-bowlW', String(d.bowler.wickets || 0));
            txt('cn-bowlR', String(d.bowler.runs || 0));
            txt('cn-bowlOv', d.bowler.overs ? '(' + d.bowler.overs + ')' : '');
        }

        // Ball dots
        renderBalls(d.recentBalls);

        // Info strip
        var infoMain = document.getElementById('cn-infoMain');
        if (infoMain) {
            if (d.isMatchEnded && d.resultMessage) {
                infoMain.innerHTML = '<span class="cn-result-txt">' + d.resultMessage + '</span>';
            } else if (activeInn === 2 && d.firstInningsScore) {
                var need = d.requiredRuns || '?';
                infoMain.innerHTML =
                    '<span class="cn-inn1-pill">' + d.firstInningsScore + '</span>' +
                    '<span class="cn-chase-txt">' + abbr(batTeam) + ' NEED ' + need + ' RUNS</span>';
            } else {
                infoMain.innerHTML = '<span class="cn-teams">' + (d.teamA || '') + ' \u2022 ' + (d.teamB || '') + '</span>';
            }
        }

        // Side panel
        if (_feat.sidepanel) {
            txt('cn-spRunRate', 'RR ' + (d.runRate || '0.00'));
            if (activeInn === 2 && d.requiredRuns) {
                txt('cn-spRRR', 'Need ' + d.requiredRuns + ' runs');
            } else {
                txt('cn-spRRR', '');
            }

            if (d.lastWicket && d.lastWicket.name) {
                var lwEl = document.getElementById('cn-spLastWicket');
                if (lwEl) lwEl.style.display = 'block';
                txt('cn-spLwName', d.lastWicket.name + ' ' + (d.lastWicket.runs || 0) + '(' + (d.lastWicket.balls || 0) + ')');
                txt('cn-spLwHow', d.lastWicket.howOut || '');
            }

            var sp = document.getElementById('cn-side-panel');
            if (sp) sp.classList.add('vis');
        }

        // Show live, hide wait
        var waitEl = document.getElementById('cn-wait');
        if (waitEl) waitEl.classList.add('hide');
        var liveEl = document.getElementById('cn-live');
        if (liveEl) liveEl.classList.add('vis');
        var ovEl = document.getElementById('cn-overlay');
        if (ovEl) ovEl.classList.add('vis');

        // --- BALL_UPDATE event (deduped) ---
        var stateKey = score + '@' + overs + '@' + activeInn;
        if (stateKey !== _lastStateKey) {
            _lastStateKey = stateKey;
            fire('BALL_UPDATE', buildStateDetails(d));
        }

        // --- New player detection ---
        if (_feat.player) {
            var curStriker = d.striker ? d.striker.name : '';
            var curNonStriker = d.nonStriker ? d.nonStriker.name : '';
            var curBowler = d.bowler ? d.bowler.name : '';

            // Only detect after first update
            if (_prevStriker || _prevNonStriker || _prevBowler) {
                // Skip: player detection for bridge-scraped data is noisy
                // We just track names for potential future use
            }

            _prevStriker = curStriker;
            _prevNonStriker = curNonStriker;
            _prevBowler = curBowler;
        }
    }

    // ========== PERIODIC STATE_SNAPSHOT ==========
    setInterval(function() {
        if (!window.CricNodeBridge || !_lastData) return;
        var d = _lastData;
        if (!d.striker && !d.bowler) return;
        fire('STATE_SNAPSHOT', buildStateDetails(d));
    }, 5000);

    // ========== PUBLIC API ==========
    window.CN = {
        init: init,
        update: update,
        setFlags: setFlags
    };

    log('Module loaded, CN namespace available');
})();
