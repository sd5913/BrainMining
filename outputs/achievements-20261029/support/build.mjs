import fs from 'node:fs/promises';
import path from 'node:path';
import sharp from 'sharp';
import {Workbook,SpreadsheetFile} from '@oai/artifact-tool';

const root='H:/PolyU/Spotlight/BrainMining';
const out=path.join(root,'outputs/achievements-20261029');
const definitions=JSON.parse(await fs.readFile(path.join(root,'Config/SteamAchievementDefinitions.json'),'utf8'));
const icons=JSON.parse(await fs.readFile(path.join(out,'support/icon_manifest.json'),'utf8'));
const conditions=[
 '正常流程首次获得首发集合内的任意藏品。重复拾取不重复计数。',
 '同一账号累计获得至少5种不同的首发藏品。',
 '同一账号获得首发固定集合的全部23件藏品（ID 0–22）。后续新增藏品不自动计入。',
 '正常购买矿镐升级，首次达到1级或以上。',
 '正常升级矿镐至4级。',
 '卡牌房进入胜利阶段并发出 OnRoomCompleted 事件。',
 '正式流程中通过博物馆出口，触发进入结局关卡的转场。',
 '正常完成结局，首次创建谢幕界面时记录通关。'
];
const concepts=['单颗灵感晶体与发现光芒','五颗相连的灵感晶体','收藏陈列柜与完整月桂环','矿镐与升级箭头','满级金色矿镐与钻石','三张元素卡牌形成连携','思考气泡、换词箭头与博物馆门','岩洞出口、星空与神经元路径'];
const wb=Workbook.create();
const main=wb.worksheets.add('成就规划');
const languages=wb.worksheets.add('后台三语文案');
const font='Microsoft YaHei';
function base(sheet,lastCol,lastRow){
 sheet.showGridLines=false;
 sheet.getRange(`A1:${lastCol}${lastRow}`).format.font={name:font,size:11,color:'#24282B'};
 sheet.getRange(`A1:${lastCol}${lastRow}`).format.verticalAlignment='center';
}
function header(sheet,range){sheet.getRange(range).format={fill:'#222A27',font:{name:font,size:11,bold:true,color:'#FFFFFF'},wrapText:true,horizontalAlignment:'center',verticalAlignment:'center',rowHeight:32};}
function title(sheet,text,context,scope,last){
 sheet.getRange('A2').values=[[text]];sheet.getRange('A2').format.font={name:font,size:17,bold:true,color:'#202824'};
 sheet.getRange('A3').values=[[context]];sheet.getRange('A3').format.font={name:font,size:10,color:'#57615A'};
 sheet.getRange('A4').values=[[scope]];sheet.getRange('A4').format.font={name:font,size:10,color:'#57615A'};
 sheet.getRange(`A5:${last}5`).format.borders={bottom:{style:'thin',color:'#B3BEB5'}};
}
base(main,'K',26);base(languages,'G',27);
main.tabColor='#314D3A';languages.tabColor='#B7C6BA';
title(main,'WOoOW 成就规划表','新增时填写名称、描述、解锁条件、是否隐藏和图标需求；API名称可先留空。','更新：2026-10-29。现有8项按项目保留；本次图标均为AI草案，尚未接入游戏或Steam。','K');
const heads=['编号','图标草案','成就名称','玩家可见描述','解锁条件（程序判定）','是否隐藏','图标需求／构思','修改意见','Steam API名称','游戏端状态','Steam后台状态'];
main.getRange('A6:K6').values=[heads];
const rows=definitions.map((d,i)=>[i+1,null,d.languages.schinese.name,d.languages.schinese.description,conditions[i],d.hidden?'是':'否',concepts[i],'',d.api_name,'已实现','待核对']);
for(let i=9;i<=20;i++)rows.push([i,null,'','','','','','','','待接入','待创建']);
main.getRange('A7:K26').values=rows;
main.tables.add('A6:K26',true,'AchievementPlanning');
header(main,'A6:K6');
const widths=[48,118,150,230,332,86,228,220,265,125,130];
widths.forEach((v,i)=>{const col=String.fromCharCode(65+i);main.getRange(`${col}1:${col}26`).format.columnWidthPx=v;});
main.getRange('A7:K14').format.rowHeightPx=108;
main.getRange('A15:K26').format.rowHeightPx=64;
main.getRange('C7:H26').format.wrapText=true;main.getRange('I7:K26').format.wrapText=true;
main.getRange('A7:B26').format.horizontalAlignment='center';main.getRange('F7:F26').format.horizontalAlignment='center';
main.getRange('C15:H26').format.fill='#FFF4CF';main.getRange('I15:I26').format.fill='#FFF4CF';
main.getRange('I7:I26').format.font={name:'Consolas',size:10,color:'#45534A'};
main.getRange('F7:F26').dataValidation={rule:{type:'list',values:['否','是']}};
main.getRange('J7:J26').dataValidation={rule:{type:'list',values:['待接入','已实现','待验证']}};
main.getRange('K7:K26').dataValidation={rule:{type:'list',values:['待核对','待创建','待发布','已发布','实机验证通过']}};
main.freezePanes.freezeRows(6);main.freezePanes.freezeColumns(3);

const grid=[];
for(let i=0;i<icons.length;i++){
 const icon=icons[i];if(!icon.source)throw new Error('Missing source '+icon.id);
 const original=await fs.readFile(icon.source);
 const full=path.join(out,'icons',icon.id+'.png');await fs.writeFile(full,original);
 const small=await sharp(original).resize(256,256).png().toBuffer();await fs.writeFile(path.join(out,'icons',icon.id+'_256.png'),small);
 main.images.add({dataUrl:'data:image/png;base64,'+small.toString('base64'),anchor:{from:{row:6+i,col:1,rowOffsetPx:6,colOffsetPx:10},extent:{widthPx:96,heightPx:96}}});
 grid.push({input:await sharp(original).resize(240,240).png().toBuffer(),left:24+(i%4)*264,top:24+Math.floor(i/4)*264});
}
await sharp({create:{width:1080,height:552,channels:3,background:'#171C19'}}).composite(grid).png().toFile(path.join(out,'图标草案总览.png'));

title(languages,'Steam 后台三语文案','简中名称、描述和API名称关联“成就规划”；英文和繁中按当前项目配置保留，可继续修改。','此表用于录入核对，不能作为Steam后台自动导入文件。现有8项均为非隐藏成就，未配置Steam Stat。','G');
languages.getRange('A6:G6').values=[['Steam API名称','简中名称','简中描述','英文名称','英文描述','繁中名称','繁中描述']];
const textRows=definitions.map(d=>[null,null,null,d.languages.english.name,d.languages.english.description,d.languages.tchinese.name,d.languages.tchinese.description]);
languages.getRange('A7:G14').values=textRows;
for(let row=7;row<=14;row++)languages.getRange(`A${row}:C${row}`).formulas=[[`='成就规划'!I${row}`,`='成就规划'!C${row}`,`='成就规划'!D${row}`]];
languages.tables.add('A6:G14',true,'SteamLanguages');header(languages,'A6:G6');
[265,155,245,200,285,155,245].forEach((v,i)=>{const c=String.fromCharCode(65+i);languages.getRange(`${c}1:${c}27`).format.columnWidthPx=v;});
languages.getRange('A7:G14').format.wrapText=true;languages.getRange('A7:G14').format.rowHeightPx=70;
languages.getRange('D7:G14').format.fill='#FFF4CF';languages.freezePanes.freezeRows(6);languages.freezePanes.freezeColumns(1);
languages.getRange('A17').values=[['填写与接入说明']];languages.getRange('A17').format.font={name:font,size:12,bold:true};
const notes=[
 ['新增内容','在“成就规划”黄色行继续添加。新成就不代表已经接入代码或后台。'],
 ['现有API名称','已经发布的API名称应保持稳定。中文改稿后需同步更新英文、繁中。'],
 ['图标草案','解锁态PNG原图和256×256预览位于随附icons目录，文件名按API名称对应。'],
 ['未解锁态图标','此次先提供每项一版解锁态草案。未解锁态及最终尺寸需在定稿时制作／核对。'],
 ['正常流程限制','PIE、试玩和自动化测试不计成就；GM授予、跳关、跳过玩法会取消本轮资格。'],
 ['来源：三语文案','Config/SteamAchievementDefinitions.json'],
 ['来源：收集与升级规则','Source/BrainMining/MiningAchievementState.cpp'],
 ['来源：事件触发','Subtitles/MainGameSubtitleDirector.cpp；WenSi/MouseMuseumPortal.cpp；EndingCredits.cpp'],
 ['Steam配置','Config/Custom/SteamPC/DefaultEngine.ini，AppID 5506030。后台状态未访问核对。']
];
languages.getRange('A18:B26').values=notes;languages.getRange('A18:B26').format.font={name:font,size:10,color:'#59615B'};
// Notes are free-flowing text outside the language table; no body-cell grid.
languages.getRange('A18:B26').format.wrapText=false;languages.getRange('A18:B26').format.rowHeightPx=28;
wb.recalculate();
const inspection=await wb.inspect({kind:'table',range:'成就规划!A6:K14',include:'values',tableMaxRows:9,tableMaxCols:11,maxChars:6000});
await fs.writeFile(path.join(out,'support/inspection.jsonl'),inspection.ndjson);
const errors=await wb.inspect({kind:'match',searchTerm:'#REF!|#DIV/0!|#VALUE!|#NAME\\?|#N/A|#NUM!|#NULL!|#SPILL!|#CALC!',options:{useRegex:true,maxResults:40},maxChars:1200});
console.log('ERROR_SCAN',errors.ndjson);
for(const [sheetName,range,file] of [['成就规划','A1:H14','planning-preview.png'],['后台三语文案','A1:G14','languages-preview.png']]){
 const preview=await wb.render({sheetName,range,scale:1,format:'png'});await fs.writeFile(path.join(out,'support',file),new Uint8Array(await preview.arrayBuffer()));
}
const file=await SpreadsheetFile.exportXlsx(wb);await file.save(path.join(out,'WOoOW_成就规划与图标草案.xlsx'));
await fs.writeFile(path.join(out,'图标生成说明.txt'),'图标由内置 image_gen 工具分别生成，为风格探索草案，未接入游戏或Steam。\n顺序与表格相同，PNG文件按Steam API名称对应。\n\n'+icons.map((v,i)=>`${i+1}. ${v.id}\n${v.prompt}`).join('\n\n'));
console.log('CREATED',path.join(out,'WOoOW_成就规划与图标草案.xlsx'));
