import fs from 'node:fs/promises';
import path from 'node:path';
import sharp from 'sharp';
import {FileBlob,SpreadsheetFile} from '@oai/artifact-tool';
const base='H:/PolyU/Spotlight/BrainMining/outputs/achievements-20261029';
const out=path.join(base,'lowpoly-v3');
await fs.mkdir(path.join(out,'icons'),{recursive:true});
const manifest=JSON.parse(await fs.readFile(path.join(base,'support/v3_manifest.json'),'utf8'));
const wb=await SpreadsheetFile.importXlsx(await FileBlob.load(path.join(base,'lowpoly-v2/WOoOW_成就规划与图标草案_lowpoly.xlsx')));
const main=wb.worksheets.getItem('成就规划');
const lang=wb.worksheets.getItem('后台三语文案');
const keep=JSON.stringify([main.getRange('C7:F14').values,main.getRange('H7:K14').values,lang.getRange('D7:G14').values]);
main.getRange('A4').values=[['更新：2026-10-29。8项已有成就＋3项新增规划。lowpoly图标为草案；新增项尚未接入游戏或Steam。']];
main.getRange('G7:G9').values=[['磁带与发现光芒，突出首次发现奇物'],['五种奇物：磁带、花、鸽子、音符、相片'],['收藏盒内陈列奇物，不使用矿石元素']];
const newData=[
 ['亮闪闪！','获得钻石。','获得1颗钻石。','','单颗浅蓝钻石与闪光','新增规划；API为建议名称；是否隐藏待定。','ACH_FIRST_DIAMOND','待接入','待创建'],
 ['工位爆破！','离开办公室，留下了炸弹。','通过办公室电脑弹窗房间。','','电脑弹窗、工位与炸弹','新增规划；按通过房间判定，不额外要求留下炸弹；API为建议名称；是否隐藏待定。','ACH_OFFICE_CLEAR','待接入','待创建'],
 ['这里是天堂？','这里是天堂吗！等等……中间那是什么？','进入天堂马桶房间。','','云朵上的白色马桶与光环','新增规划；API为建议名称；是否隐藏待定。','ACH_HEAVEN_ROOM_ENTER','待接入','待创建']
];
main.getRange('C15:K17').values=newData;
main.getRange('A15:K17').format.rowHeightPx=108;
main.getRange('H15:H17').format.font={name:'Microsoft YaHei',size:10,color:'#59615B'};
main.deleteAllDrawings();
const images=[];
for(let i=0;i<manifest.length;i++){
 const spec=manifest[i];const bytes=await fs.readFile(spec.source);
 await fs.writeFile(path.join(out,'icons',spec.id+'.png'),bytes);
 const small=await sharp(bytes).resize(256,256).png().toBuffer();
 await fs.writeFile(path.join(out,'icons',spec.id+'_256.png'),small);
 main.images.add({dataUrl:'data:image/png;base64,'+small.toString('base64'),anchor:{from:{row:6+i,col:1,rowOffsetPx:6,colOffsetPx:10},extent:{widthPx:96,heightPx:96}}});
 images.push({input:await sharp(bytes).resize(240,240).png().toBuffer(),left:24+(i%4)*264,top:24+Math.floor(i/4)*264});
}
await sharp({create:{width:1080,height:816,channels:3,background:'#1c282a'}}).composite(images).png().toFile(path.join(out,'图标总览_11项.png'));
const notes=lang.getRange('A18:B26').values;
lang.getRange('A17:B26').clear({applyTo:'all'});
lang.getRange('A21').values=[['填写与接入说明']];
lang.getRange('A21').format.font={name:'Microsoft YaHei',size:12,bold:true,color:'#24282B'};
lang.getRange('A22:B30').values=notes;
lang.getRange('A22:B30').format={font:{name:'Microsoft YaHei',size:10,color:'#59615B'},wrapText:false,rowHeightPx:28,verticalAlignment:'center'};
lang.getRange('A31:B31').values=[['本次新增3项','新增项英／繁中待补充，是否隐藏待定。API名称为建议值，未接入程序／Steam。']];
lang.getRange('A31:B31').format={font:{name:'Microsoft YaHei',size:10,color:'#59615B'},wrapText:false,rowHeightPx:28};
lang.tables.items[0].rows.add(null,newData.map(()=>[null,null,null,'待补充','待补充','待补充','待补充']));
for(let r=15;r<=17;r++)lang.getRange(`A${r}:C${r}`).formulas=[[`='成就规划'!I${r}`,`='成就规划'!C${r}`,`='成就规划'!D${r}`]];
lang.getRange('A15:G17').format={font:{name:'Microsoft YaHei',size:11,color:'#24282B'},wrapText:true,rowHeightPx:70,verticalAlignment:'center'};
lang.getRange('D15:G17').format.fill='#FFF4CF';
lang.getRange('A4').values=[['8项现有文案保留；3项新增简中按本次提供录入，英／繁中待补充。本表不能直接自动导入Steam后台。']];
if(keep!==JSON.stringify([main.getRange('C7:F14').values,main.getRange('H7:K14').values,lang.getRange('D7:G14').values]))throw new Error('Existing records changed');
wb.recalculate();
console.log((await wb.inspect({kind:'table',range:'成就规划!C15:K17',include:'values',tableMaxRows:3,tableMaxCols:9,maxChars:3000})).ndjson);
console.log((await wb.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?|#N/A|#NUM!|#NULL!',options:{useRegex:true,maxResults:20},maxChars:1000})).ndjson);
for(const [sheetName,range,file] of [['成就规划','A1:H17','v3-planning.png'],['后台三语文案','A12:G19','v3-languages.png']]){
 const img=await wb.render({sheetName,range,scale:1,format:'png'});await fs.writeFile(path.join(base,'support',file),new Uint8Array(await img.arrayBuffer()));
}
const xlsx=await SpreadsheetFile.exportXlsx(wb);
await xlsx.save(path.join(out,'WOoOW_成就规划_11项_lowpoly.xlsx'));
await fs.writeFile(path.join(out,'图标生成说明.txt'),'内置image_gen工具生成的lowpoly图标草案。\n前三项藏品成就采用奇物主题，不使用矿石元素。\n新增3项仅列入规划表，未接入游戏或Steam。\n\n'+manifest.map((v,i)=>`${i+1}. ${v.id}\n${v.prompt}`).join('\n\n'));
console.log('SAVED',out);
