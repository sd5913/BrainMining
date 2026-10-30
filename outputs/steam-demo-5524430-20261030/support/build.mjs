import fs from 'node:fs/promises';
import path from 'node:path';
import {Workbook,SpreadsheetFile,FileBlob} from '@oai/artifact-tool';

const root='H:/PolyU/Spotlight/BrainMining';
const out=path.join(root,'outputs/steam-demo-5524430-20261030');
const source='H:/BaiduNetdiskDownload';
const projectDefinitions=JSON.parse(await fs.readFile(path.join(root,'Config/SteamAchievementDefinitions.json'),'utf8'));
const plan=await SpreadsheetFile.importXlsx(await FileBlob.load(path.join(root,'outputs/achievements-20261029/lowpoly-v3/WOoOW_成就规划_11项_lowpoly.xlsx')));
const records=plan.worksheets.getItem('成就规划').getRange('C7:K17').values;
const byApi=new Map(records.map(r=>[r[6],{name:r[0],description:r[1],hidden:r[3]}]));
if(byApi.size!==11 || projectDefinitions.length!==11)throw new Error('Expected exactly eleven unique achievements');
// Updated translations follow the simplified Chinese wording approved in the planning workbook.
const translations={
 ACH_FIRST_COLLECTION:{english:{description:'Collect your first inspiration collectible.'},tchinese:{description:'收集1個靈感藏品。'}},
 ACH_COLLECTION_5:{english:{name:'Ideas Are Emerging',description:'Collect 5 different inspiration collectibles.'},tchinese:{name:'靈感正在湧現',description:'收集5個靈感藏品。'}},
 ACH_COLLECTION_COMPLETE:{english:{name:'A Burst of Inspiration',description:'Collect all inspiration collectibles.'},tchinese:{name:'靈感大爆發',description:'收集全部靈感藏品。'}},
 ACH_CARD_ROOM_CLEAR:{english:{description:'Defeat the robot in the giant room using cards.'},tchinese:{description:'使用卡牌擊敗巨大房間的機器人。'}},
 ACH_MUSEUM_CLEAR:{english:{description:'Escape the museum.'},tchinese:{description:'成功逃出了博物館。'}},
 ACH_CAMPAIGN_COMPLETE:{english:{description:'Complete the game!'},tchinese:{description:'通關遊戲！'}}
};
const definitions=projectDefinitions.map(d=>{
 const chinese=byApi.get(d.api_name);if(!chinese)throw new Error('Unknown plan API '+d.api_name);
 const item=structuredClone(d);
 if(!['是','否'].includes(chinese.hidden))throw new Error('Specify achievement visibility for '+d.api_name);
 item.hidden=chinese.hidden==='是';
 if(item.hidden!==d.hidden)throw new Error('Planning visibility and project config disagree: '+d.api_name);
 item.languages.schinese={name:chinese.name,description:chinese.description};
 for(const culture of ['english','tchinese'])Object.assign(item.languages[culture],translations[d.api_name]?.[culture]??{});
 return item;
});
const names=['5524430-achievements-definitions.csv','5524430-achievement-sets-localization.csv','5524430-achievements-localization.csv'];
const matrices=[
 definitions.map(d=>[d.api_name,'','Client',d.hidden?'true':'false','false','',0,0]),
 [],
 definitions.flatMap(d=>['name','description'].map(field=>[d.api_name,field,...['english','schinese','tchinese'].map(c=>d.languages[c][field])]))
];
function encode(rows){
 const cell=v=>{const s=v==null?'':String(v);return /[",\r\n]/.test(s)?'"'+s.replaceAll('"','""')+'"':s;};
 return '\ufeff'+rows.map(r=>r.map(cell).join(',')).join('\r\n')+'\r\n';
}
const schemas=[];
for(let i=0;i<names.length;i++){
 const original=await fs.readFile(path.join(source,names[i]),'utf8');
 const wb=await Workbook.fromCSV(original.replace(/^\ufeff/,''),{sheetName:'SteamImport'});
 const sheet=wb.worksheets.getItemAt(0);
 const width=[8,4,5][i];const last=String.fromCharCode(64+width);
 const header=sheet.getRange(`A1:${last}1`).values[0];schemas.push(header);
 sheet.getRange(`A2:${last}${i===0?3:i===2?5:2}`).clear({applyTo:'contents'});
 if(matrices[i].length)sheet.getRange(`A2:${last}${matrices[i].length+1}`).values=matrices[i];
 wb.recalculate();
 const rows=sheet.getRange(`A1:${last}${matrices[i].length+1}`).values;
 if(rows.length!==matrices[i].length+1 || rows[0].some((v,j)=>v!==header[j]))throw new Error('Template shape changed');
 await fs.writeFile(path.join(out,names[i]),encode(rows),'utf8');
 const inspected=await wb.inspect({kind:'table',range:`A1:${last}${Math.min(rows.length,4)}`,include:'values',tableMaxRows:4,tableMaxCols:width,maxChars:2200});
 await fs.writeFile(path.join(out,'support',names[i]+'.inspect.jsonl'),inspected.ndjson);
 // CSV has no visual formatting. Render only a bounded review view, never export this styling.
 sheet.getRange(`A1:${last}${Math.max(rows.length,2)}`).format={font:{name:'Microsoft YaHei',size:11,color:'#202824'},wrapText:true,verticalAlignment:'center'};
 sheet.getRange(`A1:${last}1`).format={fill:'#26352D',font:{name:'Microsoft YaHei',size:11,bold:true,color:'#FFFFFF'},rowHeightPx:i===0?110:70};
 sheet.getRange(`A1:${last}${Math.max(rows.length,2)}`).format.columnWidthPx=i===0?195:230;
 sheet.getRange('A1:A23').format.columnWidthPx=280;
 if(rows.length>1)sheet.getRange(`A2:${last}${rows.length}`).format.rowHeightPx=55;
 const preview=await wb.render({sheetName:'SteamImport',range:`A1:${last}${Math.min(rows.length,5)}`,scale:1,format:'png'});
 await fs.writeFile(path.join(out,'support',names[i]+'.png'),new Uint8Array(await preview.arrayBuffer()));
}
await fs.writeFile(path.join(out,'support/content.json'),JSON.stringify({app_id:5524430,definitions,schemas},null,2),'utf8');
await fs.writeFile(path.join(out,'导入说明.txt'),[
 'Steam Demo 成就导入包 — App ID 5524430',
 '2026-10-30',
 '',
 '1. 在 Demo 5524430 的 Steamworks 成就配置页面，先导入 achievements-definitions.csv（11项）。',
 '2. 再导入 achievements-localization.csv（22行，英／简中／繁中名称和描述）。',
 '3. achievement-sets-localization.csv 只有原始表头：目前11项不归属自定义集合，无集合ID，不需要导入此表。',
 '',
 '11项均由客户端解锁、未归档、不绑定统计数据或进度条。进度最小／最大值沿用模板的无统计数据示例：0／0。',
 '其中4项为隐藏成就（spoiler=true）：组合的力量、换个想法、工位爆破！、这里是天堂？；其余7项为非隐藏成就。',
 '成就API名称与游戏现有11项完全一致。简中文案采用最新成就规划表，英文和繁中同步对应。',
 '图标不在这三个模板的字段中，需要在Steamworks中按成就API名称另行上传。',
 '检查导入预览无误后保存并发布配置，再从Steam库启动Demo测试。',
 '',
 '项目 SteamPC 的 SteamDevAppId 和 ExpectedAppId 已改为5524430；此设置需重新打包SteamPC版本后生效。',
 '本次没有启动UE、重新打包或操作Steamworks后台。原始模板保留未覆盖。',
 ''
].join('\r\n'),'utf8');
console.log(JSON.stringify({app_id:5524430,achievements:definitions.length,localization_rows:matrices[2].length,sets:0,files:names}));
