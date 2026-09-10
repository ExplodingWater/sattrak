import json,urllib.request,urllib.parse,concurrent.futures
from pathlib import Path
root=Path('assets/stations');root.mkdir(parents=True,exist_ok=True)
def get(url):
 return urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'AeroStudio/1.0 (educational desktop simulation)'}),timeout=40).read()
def download(item):
 key,title=item
 summary=json.loads(get('https://en.wikipedia.org/api/rest_v1/page/summary/'+urllib.parse.quote(title)))
 source=summary['thumbnail']['source'].split('?')[0]
 image=get(source)
 (root/(key+'.jpg')).write_bytes(image)
 original=summary['originalimage']['source'].split('?')[0]
 filename=urllib.parse.unquote(source.split('/thumb/')[1].split('/')[2])
 result={'file':key+'.jpg','location':title,'image_url':source,'source_page':'https://commons.wikimedia.org/wiki/File:'+urllib.parse.quote(filename),'width':summary['thumbnail']['width'],'height':summary['thumbnail']['height']}
 try:
  url='https://commons.wikimedia.org/w/api.php?'+urllib.parse.urlencode({'action':'query','format':'json','prop':'imageinfo','iiprop':'extmetadata','titles':'File:'+filename})
  meta=json.loads(get(url));result['attribution']=next(iter(meta['query']['pages'].values()))['imageinfo'][0]['extmetadata']
 except Exception as e: result['metadata_error']=str(e)
 print(key,len(image),result['source_page'],flush=True)
 (root/(key+'.json')).write_text(json.dumps(result,indent=2),encoding='utf-8')
 return result
items=[('cape','Cape Canaveral'),('houston','Houston'),('london','London'),('tokyo','Tokyo'),('sydney','Sydney'),('svalbard','Svalbard Satellite Station'),('kourou','Kourou')]
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
 for item,result in zip(items,pool.map(download,items)): pass
