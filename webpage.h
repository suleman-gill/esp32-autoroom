#pragma once
#include <pgmspace.h>

// Self-contained mobile web app (no external files, works fully offline)
const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0b1020">
<title>Auto Room</title>
<style>
:root{--bg:#0b1020;--card:#151b30;--card2:#1e2646;--tx:#e8ecf8;--mut:#8a94b8;--acc:#4f8cff;--on:#ffc43d;--bad:#ff5d5d;--ok:#3ddc97}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;background:var(--bg);color:var(--tx);font:16px system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;padding:max(16px,env(safe-area-inset-top)) 16px calc(28px + env(safe-area-inset-bottom))}
.wrap{max-width:560px;margin:0 auto}
header{display:flex;align-items:center;justify-content:space-between;margin-bottom:14px}
h1{font-size:1.35rem;margin:0}
.sub{color:var(--mut);font-size:.8rem;display:flex;align-items:center;gap:6px;margin-top:2px}
.dot{width:8px;height:8px;border-radius:50%;background:var(--bad)}
.dot.ok{background:var(--ok)}
button{font:inherit;color:inherit;border:0;cursor:pointer}
.gear{background:var(--card);border-radius:12px;width:42px;height:42px;display:grid;place-items:center}
svg{width:22px;height:22px;fill:none;stroke:currentColor;stroke-width:1.8;stroke-linecap:round;stroke-linejoin:round}
.scenes{display:flex;gap:8px;overflow-x:auto;padding-bottom:6px;margin-bottom:12px;scrollbar-width:none}
.scenes::-webkit-scrollbar{display:none}
.scenes button{flex:0 0 auto;background:var(--card);padding:10px 14px;border-radius:999px;font-size:.88rem}
.scenes button:active{background:var(--acc)}
.grid{display:grid;gap:12px}
.card{background:var(--card);border-radius:18px;padding:14px;border:1px solid transparent;transition:border-color .2s,background .2s}
.card.on{border-color:rgba(255,196,61,.45);background:linear-gradient(160deg,#1c2240,#2a2a3a)}
.top{display:flex;align-items:center;gap:12px}
.ico{width:46px;height:46px;border-radius:14px;background:var(--card2);display:grid;place-items:center;color:var(--mut);flex:0 0 auto;transition:.2s}
.card.on .ico{color:var(--on);background:rgba(255,196,61,.14)}
.nm{flex:1;min-width:0;display:flex;align-items:center;gap:4px}
.name{font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.ed{background:none;color:var(--mut);padding:6px;display:grid;place-items:center}
.ed svg{width:16px;height:16px}
.sw{width:56px;height:32px;border-radius:999px;background:#2b3354;position:relative;flex:0 0 auto;transition:background .2s}
.sw i{position:absolute;top:3px;left:3px;width:26px;height:26px;border-radius:50%;background:#fff;transition:transform .2s}
.card.on .sw{background:var(--on)}
.card.on .sw i{transform:translateX(24px)}
.tm{margin-top:12px;display:flex;align-items:center;gap:8px;flex-wrap:wrap}
.cd{font-variant-numeric:tabular-nums;color:var(--on);font-size:.85rem;min-width:70px}
.chips{display:flex;gap:6px;flex-wrap:wrap}
.chip{background:var(--card2);color:var(--mut);padding:6px 10px;border-radius:999px;font-size:.78rem}
.chip:active{background:var(--acc);color:#fff}
.chip.x{color:var(--bad);display:none}
.card.timed .chip.x{display:inline-block}
.toast{position:fixed;left:50%;bottom:calc(20px + env(safe-area-inset-bottom));transform:translate(-50%,30px);background:#2b3354;padding:10px 16px;border-radius:12px;opacity:0;pointer-events:none;transition:.25s;font-size:.9rem;max-width:90%}
.toast.show{opacity:1;transform:translate(-50%,0)}
dialog{background:var(--card);color:var(--tx);border:0;border-radius:18px;padding:18px;width:min(92vw,380px)}
dialog::backdrop{background:rgba(0,0,0,.6)}
dialog h2{margin:0 0 4px;font-size:1.1rem}
dialog p{color:var(--mut);font-size:.82rem;margin:0 0 12px}
input{width:100%;background:var(--bg);border:1px solid #2b3354;color:var(--tx);border-radius:10px;padding:11px;margin-bottom:10px;font:inherit}
.row{display:flex;gap:8px;justify-content:flex-end}
.btn{background:var(--card2);padding:10px 16px;border-radius:10px}
.btn.p{background:var(--acc);color:#fff}
@media (prefers-color-scheme:light){:root{--bg:#eef1f8;--card:#fff;--card2:#eef1f8;--tx:#151b30;--mut:#6b7494}.card.on{background:#fff8e6}.sw{background:#cfd5ea}}
</style></head>
<body><div class="wrap">
<header>
  <div><h1>Auto Room</h1><div class="sub"><span class="dot" id="dot"></span><span id="info">Connecting...</span></div></div>
  <button class="gear" id="gear" aria-label="Wi-Fi settings"><svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.7 1.7 0 0 0 .3 1.8l.1.1a2 2 0 1 1-2.8 2.8l-.1-.1a1.7 1.7 0 0 0-1.8-.3 1.7 1.7 0 0 0-1 1.5V21a2 2 0 1 1-4 0v-.1a1.7 1.7 0 0 0-1.1-1.5 1.7 1.7 0 0 0-1.8.3l-.1.1a2 2 0 1 1-2.8-2.8l.1-.1a1.7 1.7 0 0 0 .3-1.8 1.7 1.7 0 0 0-1.5-1H3a2 2 0 1 1 0-4h.1a1.7 1.7 0 0 0 1.5-1.1 1.7 1.7 0 0 0-.3-1.8l-.1-.1a2 2 0 1 1 2.8-2.8l.1.1a1.7 1.7 0 0 0 1.8.3H9a1.7 1.7 0 0 0 1-1.5V3a2 2 0 1 1 4 0v.1a1.7 1.7 0 0 0 1 1.5 1.7 1.7 0 0 0 1.8-.3l.1-.1a2 2 0 1 1 2.8 2.8l-.1.1a1.7 1.7 0 0 0-.3 1.8V9a1.7 1.7 0 0 0 1.5 1H21a2 2 0 1 1 0 4h-.1a1.7 1.7 0 0 0-1.5 1z"/></svg></button>
</header>

<div class="scenes" id="scenes">
  <button data-a="all_on">All ON</button>
  <button data-a="all_off">All OFF</button>
  <button data-a="night">Night</button>
  <button data-a="movie">Movie</button>
  <button data-a="reading">Reading</button>
</div>

<div class="grid" id="grid"></div>
</div>

<dialog id="dlg">
  <h2>Wi-Fi settings</h2>
  <p>Enter your 2.4 GHz router details. The controller will restart.</p>
  <input id="ssid" placeholder="Wi-Fi name (SSID)" autocapitalize="none" autocomplete="off" maxlength="32">
  <input id="pass" type="password" placeholder="Password (8-63 characters)" autocomplete="off" maxlength="63">
  <div class="row"><button class="btn" id="cancel">Cancel</button><button class="btn p" id="save">Save</button></div>
</dialog>
<div class="toast" id="toast"></div>

<script>
const $=s=>document.querySelector(s);
const BULB='<svg viewBox="0 0 24 24"><path d="M9 18h6M10 21h4M12 3a6 6 0 0 0-3.5 10.9c.6.5 1 1.3 1 2.1h5c0-.8.4-1.6 1-2.1A6 6 0 0 0 12 3z"/></svg>';
const FAN='<svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="1.8"/><path d="M12 10.2C12 6 10.5 3 8 3.6c-2 .5-1.6 3.6 1.4 5.4M13.8 12c4.200 0 7.200-1.500 6.600-4-.5-2-3.600-1.600-5.400 1.400M12 13.800c0 4.200 1.500 7.200 4 6.600 2-.5 1.600-3.600-1.400-5.400M10.200 12C6 12 3 13.500 3.600 16c.5 2 3.600 1.600 5.400-1.400"/></svg>';
const EDIT='<svg viewBox="0 0 24 24"><path d="M12 20h9M16.500 3.500a2.100 2.100 0 0 1 3 3L7 19l-4 1 1-4z"/></svg>';
const CHIPS=[[15,'15m'],[30,'30m'],[60,'1h'],[120,'2h']];
let data=null,busy=0,toastT;

function toast(m){const t=$('#toast');t.textContent=m;t.classList.add('show');clearTimeout(toastT);toastT=setTimeout(()=>t.classList.remove('show'),2200)}
function fmt(s){const h=Math.floor(s/3600),m=Math.floor(s%3600/60),x=s%60,p=n=>String(n).padStart(2,'0');return h?h+':'+p(m)+':'+p(x):p(m)+':'+p(x)}

async function api(path,params){
  const r=await fetch(path,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(params)});
  const t=await r.text();
  if(!r.ok)throw new Error(t||r.status);
  return t;
}

function build(){
  const g=$('#grid');g.innerHTML='';
  data.relays.forEach((r,i)=>{
    const c=document.createElement('div');
    c.className='card';c.dataset.id=r.id;
    c.innerHTML='<div class="top"><div class="ico">'+(i===3?FAN:BULB)+'</div>'+
      '<div class="nm"><span class="name"></span><button class="ed" aria-label="Rename">'+EDIT+'</button></div>'+
      '<button class="sw" aria-label="Toggle"><i></i></button></div>'+
      '<div class="tm"><span class="cd"></span><div class="chips">'+
      CHIPS.map(x=>'<button class="chip" data-m="'+x[0]+'">'+x[1]+'</button>').join('')+
      '<button class="chip x" data-m="0">Cancel</button></div></div>';
    g.appendChild(c);
  });
}

function render(){
  if($('#grid').children.length!==data.relays.length)build();
  data.relays.forEach(r=>{
    const c=$('#grid [data-id="'+r.id+'"]');
    c.classList.toggle('on',r.state);
    c.classList.toggle('timed',r.timerSeconds>0);
    c.querySelector('.name').textContent=r.name;
    c.querySelector('.cd').textContent=r.timerSeconds>0?'Off in '+fmt(r.timerSeconds):'';
  });
  $('#dot').classList.add('ok');
  $('#info').textContent=(data.mode==='wifi'?'Wi-Fi ':'Hotspot ')+data.ip+(data.rssi?' ('+data.rssi+' dBm)':'');
}

async function poll(force){
  if(!force&&(document.hidden||busy))return;
  try{
    const r=await fetch('/api/status',{cache:'no-store'});
    if(!r.ok)throw 0;
    data=await r.json();render();
  }catch(e){$('#dot').classList.remove('ok');$('#info').textContent='Offline - retrying...'}
}

async function act(fn){
  busy++;
  try{await fn()}catch(e){toast('Failed: '+(e.message||'error'))}
  busy--;
  await poll(true);
}

$('#grid').addEventListener('click',e=>{
  const c=e.target.closest('.card');if(!c||!data)return;
  const id=c.dataset.id,r=data.relays[id-1];
  if(e.target.closest('.sw'))act(()=>api('/api/relay',{id:id,state:r.state?0:1}));
  else if(e.target.closest('.ed')){
    const n=prompt('Rename appliance',r.name);
    if(n&&n.trim())act(()=>api('/api/rename',{id:id,name:n.trim()}));
  }else if(e.target.closest('.chip')){
    act(()=>api('/api/timer',{id:id,minutes:e.target.closest('.chip').dataset.m}));
  }
});

$('#scenes').addEventListener('click',e=>{
  const b=e.target.closest('button');if(!b)return;
  act(()=>api('/api/master',{action:b.dataset.a}));
});

const dlg=$('#dlg');
$('#gear').onclick=()=>dlg.showModal();
$('#cancel').onclick=()=>dlg.close();
$('#save').onclick=async()=>{
  const ssid=$('#ssid').value.trim(),pass=$('#pass').value;
  if(!ssid){toast('Enter the Wi-Fi name');return}
  try{
    await api('/api/wifi',{ssid:ssid,pass:pass});
    dlg.close();toast('Saved. Restarting... reconnect in ~20s');
  }catch(e){toast('Failed: '+(e.message||'error'))}
};

// local countdown between polls
setInterval(()=>{
  if(!data)return;
  data.relays.forEach(r=>{
    if(r.timerSeconds>0){
      r.timerSeconds--;
      const c=$('#grid [data-id="'+r.id+'"]');
      if(c)c.querySelector('.cd').textContent=r.timerSeconds>0?'Off in '+fmt(r.timerSeconds):'';
    }
  });
},1000);

setInterval(poll,1500);
document.addEventListener('visibilitychange',()=>{if(!document.hidden)poll(true)});
poll(true);
</script></body></html>)rawliteral";
