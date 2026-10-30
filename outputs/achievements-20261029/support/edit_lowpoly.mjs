import fs from 'node:fs/promises';
import path from 'node:path';
import sharp from 'sharp';
import {FileBlob,SpreadsheetFile} from '@oai/artifact-tool';

const base='H:/PolyU/Spotlight/BrainMining/outputs/achievements-20261029';
const out=path.join(base,'lowpoly-v2');
await fs.mkdir(path.join(out,'icons'),{recursive:true});
const icons=JSON.parse(await fs.readFile(path.join(base,'support/lowpoly_manifest.json'),'utf8'));
const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(path.join(base,'WOoOW_成就规划与图标草案.xlsx')));
const main=wb.worksheets.getItem('成就规划');
const before=JSON.stringify({a:main.getRange('C7:F26').values,b:main.getRange('H7:K26').values,l:wb.worksheets.getItem('后台三语文案').getRange('A7:G14').formulas});
console.log((await wb.inspect({kind:'sheet',include:'id,name',maxChars:1200})).ndjson);
main.deleteAllDrawings();
main.getRange('A4').values=[['更新：2026-10-29。现有8项按项目保留；图标已改为lowpoly草案，尚未接入游戏或Steam。']];
main.getRange('G7:G14').values=[['低多边形灵感晶体与发现光芒'],['五组不同颜色的低多边形晶体'],['装满不同藏品的简洁陈列盒'],['块面矿镐与单个升级箭头'],['满级块面矿镐与钻石'],['三张元素卡牌形成连携'],['思考气泡、换词箭头与方柱博物馆门'],['多边形岩洞出口、石阶与星空']];
const composites=[];
for(let i=0;i<icons.length;i++){
 const icon=icons[i];
 const bytes=await fs.readFile(icon.source);
 await fs.writeFile(path.join(out,'icons',icon.id+'.png'),bytes);
 const small=await sharp(bytes).resize(256,256).png().toBuffer();
 await fs.writeFile(path.join(out,'icons',icon.id+'_256.png'),small);
 main.images.add({dataUrl:'data:image/png;base64,'+small.toString('base64'),anchor:{from:{row:6+i,col:1,rowOffsetPx:6,colOffsetPx:10},extent:{widthPx:96,heightPx:96}}});
 composites.push({input:await sharp(bytes).resize(240,240).png().toBuffer(),left:24+(i%4)*264,top:24+Math.floor(i/4)*264});
}
await sharp({create:{width:1080,height:552,channels:3,background:'#1c282a'}}).composite(composites).png().toFile(path.join(out,'图标草案总览_lowpoly.png'));
const after=JSON.stringify({a:main.getRange('C7:F26').values,b:main.getRange('H7:K26').values,l:wb.worksheets.getItem('后台三语文案').getRange('A7:G14').formulas});
if(before!==after)throw new Error('Unrelated achievement values or language formulas changed');
wb.recalculate();
console.log((await wb.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?|#N/A|#NUM!|#NULL!',options:{useRegex:true,maxResults:20},maxChars:1000})).ndjson);
const preview=await wb.render({sheetName:'成就规划',range:'A1:H14',scale:1,format:'png'});
await fs.writeFile(path.join(base,'support/lowpoly-preview.png'),new Uint8Array(await preview.arrayBuffer()));
const xlsx=await SpreadsheetFile.exportXlsx(wb);
await xlsx.save(path.join(out,'WOoOW_成就规划与图标草案_lowpoly.xlsx'));
await fs.writeFile(path.join(out,'图标生成说明.txt'),'内置 image_gen 工具生成。第二版：低多边形、纯色块面、哑光、简洁轮廓。\n本次仅更新文档和图标草案，未启动UE，未接入游戏或Steam。\n\n'+icons.map((v,i)=>`${i+1}. ${v.id}\n${v.prompt}`).join('\n\n'));
console.log('UPDATED',out);
