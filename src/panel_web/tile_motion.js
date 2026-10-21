'use strict';
(function(root){
 function fanPeriod(power){
  if(power===null||power===undefined)return 2.4;
  return typeof power==='number'&&Number.isFinite(power)&&power>0&&power<=100?80/power:null;
 }
 if(typeof module!=='undefined'&&module.exports)module.exports={fanPeriod};
 if(typeof document==='undefined')return;
 const width=48,height=64,heat=new Uint8Array(width*height);
 const reduced=window.matchMedia('(prefers-reduced-motion: reduce)');
 let container=null,canvas=null,context=null,pixels=null,frame=0,last=0,lastPower=null;
 function stop(){if(frame)cancelAnimationFrame(frame);frame=0;last=0;}
 function step(){
  for(let y=0;y<height-1;y++)for(let x=1;x<width-1;x++){
   const drift=Math.max(1,Math.min(width-2,x+Math.floor(Math.random()*3)-1));
   const below=(y+1)*width+drift,deeper=Math.min(height-1,y+2)*width+drift;
   heat[y*width+x]=Math.max(0,(heat[below]*2+heat[deeper-1]+heat[deeper+1])/4-Math.random()*11);
  }
  for(let x=0;x<width;x++)heat[(height-1)*width+x]=Math.abs(x-(width-1)/2)<14+Math.random()*3?205+Math.random()*50:0;
 }
 function paint(){
  for(let i=0;i<heat.length;i++){
   const t=heat[i],p=i*4;
   pixels.data[p]=255;pixels.data[p+1]=Math.min(255,Math.max(0,(t-55)*1.4));
   pixels.data[p+2]=Math.max(0,(t-180)*2.6);pixels.data[p+3]=Math.min(255,Math.max(0,(t-35)*5));
  }
  context.putImageData(pixels,0,0);
 }
 function tick(now){
  frame=0;
  if(!canvas?.isConnected||!canvas.closest('[data-running=true]')||!canvas.getClientRects().length)return;
  if(now-last>=50){step();paint();last=now;}
  frame=requestAnimationFrame(tick);
 }
 function render(element,power){
  container=element;lastPower=power;
  const fan=container.querySelector('[data-member=fanPower]');
  if(fan)fan.style.setProperty('--fan-period',(fanPeriod(power)??2.4)+'s');
  const graphic=container.querySelector('[data-member=boilerPower] .tile-symbol');
  if(!graphic){stop();return;}
  if(canvas?.parentElement!==graphic){
   stop();canvas=document.createElement('canvas');canvas.width=width;canvas.height=height;
   canvas.setAttribute('aria-hidden','true');canvas.className='tile-fire';
   context=canvas.getContext('2d');if(!context)return;
   pixels=context.createImageData(width,height);heat.fill(0);
   for(let i=0;i<100;i++)step();paint();
   graphic.classList.add('fire-symbol');graphic.append(canvas);
  }
  if(!graphic.closest('[data-running=true]')||reduced.matches){stop();return;}
  if(!frame)frame=requestAnimationFrame(tick);
 }
 reduced.addEventListener('change',()=>{if(container)render(container,lastPower);});
 root.EcoTileMotion={render,pause:stop,fanPeriod};
})(globalThis);
