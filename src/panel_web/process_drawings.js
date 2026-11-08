(function(root){
 'use strict';
 const circulation=`
 <path class="pipe-case hot-shell"/><path class="pipe-case return-shell"/>
 <path class="water hot-water"/><path class="water return-water"/>
 <path class="flow hot-flow"/><path class="flow return-flow"/>
 <path class="tap-branch pipe-case"/><path class="tap-feed water"/>
 <g class="tank-unit">
  <path class="equipment-foot" d="M36 251v10m56-10v10"/>
  <rect class="equipment-body" x="22" y="69" width="84" height="184" rx="18"/>
  <path class="equipment-rule" d="M23 94h82M23 228h82"/>
  <rect class="tank-insulation" x="28" y="103" width="6" height="115" rx="3"/>
  <rect class="tank-hot" x="41" y="151" width="46" height="27" rx="4"/>
  <rect class="tank-cool" x="41" y="183" width="46" height="28" rx="4"/>
  <path class="tank-thermometer" d="M64 161v34m-5-28h5m-5 9h5m-5 9h5"/>
  <circle class="thermometer-end" cx="64" cy="196" r="4"/>
  <text class="equipment-title" x="64" y="132" text-anchor="middle">CWU</text>
  <path class="tank-port" d="M106 115h8M106 230h8"/>
 </g>
 <g class="tap-unit">
  <path class="tap-outline" d="M0 0v-34c0-17-10-28-26-28s-26 11-26 28v6"/>
  <path class="tap-metal" d="M0 0v-34c0-17-10-28-26-28s-26 11-26 28v6"/>
  <path class="tap-base equipment-body" d="M-10-3h20v8h-20z"/>
  <path class="tap-handle" d="M4-15h13v-13"/>
  <path class="tap-aerator" d="M-58-27h12"/>
 </g>
 <text class="tap-label equipment-caption" text-anchor="middle">Kran</text>
 <g class="pump-unit">
  <path class="pump-coupling equipment-body" d="M-35-7h70v14h-70zM-34-11h8v22h-8zM26-11h8v22h-8z"/>
  <circle class="pump-rim" r="25"/><circle class="pump-face" r="18"/>
  <g class="pump-rotor"><path d="M0-3c-10-10-14 1-8 6l8-3M3 0c10-10-1-14-6-8l3 8M0 3c10 10 14-1 8-6l-8 3M-3 0c-10 10 1 14 6 8l-3-8"/><circle r="3"/></g>
  <circle class="pump-led" cx="0" cy="21" r="2"/>
 </g>
 <path class="hot-arrow pipe-direction"/><path class="return-arrow pipe-direction"/>
 <text class="pump-label equipment-caption" text-anchor="middle">Pompa cyrkulacyjna</text>`;
 const feeder=`
 <defs><clipPath id="auger-tube-clip"><rect class="clip-box" x="78" y="139" height="44" rx="5"/></clipPath>
 <clipPath id="auger-hopper-clip"><path d="M76 52h102v49l-35 37h-31l-36-37z"/></clipPath></defs>
 <g class="feeder-hopper">
  <path class="equipment-body" d="M74 48h106v55l-36 38v16h-34v-16l-36-38z"/>
  <g clip-path="url(#auger-hopper-clip)">
   <path class="hopper-pellet-bed" d="M76 83l30-7 30 8 42-5v64H76z"/>
   <g class="hopper-grains grain">
    <rect x="83" y="85" width="14" height="5" rx="2" transform="rotate(-15 90 88)"/><rect x="104" y="82" width="14" height="5" rx="2" transform="rotate(17 111 85)"/>
    <rect x="126" y="89" width="14" height="5" rx="2" transform="rotate(-20 133 92)"/><rect x="150" y="86" width="14" height="5" rx="2"/>
    <rect x="93" y="99" width="14" height="5" rx="2" transform="rotate(18 100 102)"/><rect x="115" y="103" width="14" height="5" rx="2" transform="rotate(-9 122 106)"/>
    <rect x="143" y="100" width="14" height="5" rx="2" transform="rotate(27 150 103)"/><rect x="111" y="120" width="14" height="5" rx="2"/>
    <rect x="132" y="117" width="14" height="5" rx="2" transform="rotate(-22 139 120)"/>
   </g>
  </g>
  <rect class="hopper-lid" x="70" y="43" width="114" height="7" rx="2"/>
  <text class="equipment-caption" x="126" y="25" text-anchor="middle">Zasobnik</text>
 </g>
 <rect class="tube equipment-body" x="72" y="135" height="52" rx="7"/>
 <g clip-path="url(#auger-tube-clip)">
  <path class="shaft"/>
  <g class="auger-spiral auger-motion"><path class="helix"/></g>
  <g class="feed-grains">
   <rect class="grain auger-grains auger-motion" x="105" y="171" width="12" height="5" rx="2"/>
   <rect class="grain auger-grains auger-motion" x="105" y="170" width="12" height="5" rx="2" style="animation-delay:-2.4s"/>
   <rect class="grain auger-grains auger-motion" x="105" y="173" width="12" height="5" rx="2" style="animation-delay:-4.8s"/>
   <rect class="grain auger-grains auger-motion" x="105" y="170" width="12" height="5" rx="2" style="animation-delay:-7.2s"/>
  </g>
 </g>
 <path class="tube-lower-seam equipment-rule"/>
 <path class="outlet-case"/><path class="outlet"/>
 <g class="motor-unit"><path class="shaft-coupling" d="M61 161h16"/><rect class="motor-body" x="20" y="140" width="44" height="42" rx="5"/>
  <path class="motor-fins" d="M30 149v24m8-24v24m8-24v24"/><rect class="motor-cap" x="56" y="143" width="8" height="36" rx="2"/>
  <path class="equipment-foot" d="M27 185h33"/><text class="equipment-caption" x="42" y="213" text-anchor="middle">Napęd</text>
 </g>
 <g class="burner-unit"><path class="burner-body" d="M-35 0v27h70V0M-40 0h80"/><path class="burner-grate" d="M-22 17h44m-34-6v6m12-6v6m12-6v6"/></g>
 <g class="auger-falling"><rect class="grain auger-drop auger-motion" width="10" height="5" rx="2"/><rect class="grain auger-drop auger-motion" width="10" height="5" rx="2" style="animation-delay:-.8s"/></g>
 <text class="auger-label equipment-caption" text-anchor="middle">Ślimak podajnika</text>
 <text class="burner-label equipment-caption" text-anchor="middle">Do palnika</text>`;
 function mount(kind,container){
  const svg=container.querySelector('svg');svg.classList.add('plant-diagram');svg.innerHTML=kind==='circulation'?circulation:feeder;
  const set=(selector,values)=>{const node=svg.querySelector(selector);for(const [key,value]of Object.entries(values))node.setAttribute(key,value);};
  const paths=(names,d)=>names.forEach(name=>set(name,{d}));
  function draw(){
   const w=Math.max(300,Math.round(container.clientWidth));
   svg.setAttribute('viewBox',`0 0 ${w} 285`);
   if(kind==='circulation'){
    const right=w-24,mid=(114+right)/2,tap=right-25;
    paths(['.hot-shell','.hot-water','.hot-flow'],`M114 115H${right-10}q10 0 10 10v40`);
    paths(['.return-shell','.return-water','.return-flow'],`M${right} 165v55q0 10-10 10H114`);
    paths(['.tap-branch','.tap-feed'],`M${tap} 115V92`);
    set('.tap-unit',{transform:`translate(${tap} 86)`});set('.tap-label',{x:tap-26,y:13});
    set('.pump-unit',{transform:`translate(${mid} 230)`});set('.pump-label',{x:mid,y:276});
    set('.hot-arrow',{d:`M${mid-14} 111l5 4-5 4`});set('.return-arrow',{d:`M${right-31} 226l-5 4 5 4`});
   }else{
    const end=w-88;
    container.style.setProperty('--travel',`${end-105}px`);
    set('.tube',{width:end-72});set('.clip-box',{width:end-78});set('.shaft',{d:`M78 161H${end}`});
    let d='M-40 145';for(let x=-40;x<w+40;x+=40)d+=`C${x+10} 145 ${x+10} 177 ${x+20} 177S${x+30} 145 ${x+40} 145`;
    set('.helix',{d});set('.tube-lower-seam',{d:`M80 182H${end-8}`});
    paths(['.outlet-case','.outlet'],`M${end} 161h20q9 0 9 9v35`);
    set('.burner-unit',{transform:`translate(${end+29} 233)`});
    for(const node of svg.querySelectorAll('.auger-drop')){node.setAttribute('x',end+24);node.setAttribute('y',207);}
    set('.auger-label',{x:(80+end)/2,y:214});set('.burner-label',{x:end+29,y:279});
   }
  }
  new ResizeObserver(draw).observe(container);draw();
 }
 root.EcoDrawings={mount};
})(globalThis);
