"""Compare all 47 models against unchanged vendored math.

Host times only rank these synthetic host cases; they are not device CPU scores.
All captures and compiler outputs belong in a fresh external directory.
"""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
MACHINE=Path(__file__).resolve().parents[1]

def require(ok,message):
    if not ok:raise ValueError(message)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('sdk','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--cxx',default='clang++')
    p.add_argument('--reference-engine',type=Path,required=True,help='Untouched engine.cpp from the baseline revision')
    a=p.parse_args()
    require(not a.out.exists() and not any((d/'.git').exists() for d in [a.out.resolve(),*a.out.resolve().parents]),'Fresh external output required')
    a.out.mkdir(parents=True)
    source_names=json.loads((MACHINE/'machine.json').read_text())['sources']
    sources=[MACHINE/'engine.cpp',*[MACHINE/n for n in source_names],MACHINE/'performance/fixture.cpp',MACHINE/'performance/host.cpp']
    def run(cmd,name):
        r=subprocess.run(list(map(str,cmd)),capture_output=True,timeout=300,creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
        (a.out/(name+'.log')).write_bytes(r.stdout+r.stderr)
        require(r.returncode==0,name+' failed; inspect private log');return r.stdout
    outputs={}
    for name,extra in [('reference',[]),('candidate',['-I'+str(MACHINE/'optimized')])]:
        exe=a.out/(name+'.exe')
        overlays={'optimized/'+kind+'_oscillator.cpp':'vendor/braids/'+kind+'_oscillator.cc' for kind in ('macro','analog','digital')}
        overlays['optimized/resources.cpp']='vendor/braids/resources.cc'
        reference_names=[overlays.get(n,n) for n in source_names if n in overlays or not n.startswith('optimized/')]
        units=[a.reference_engine,*[MACHINE/n for n in reference_names],*sources[-2:]] if name=='reference' else sources
        run([a.cxx,'-std=c++11','-O2','-fwrapv','-I'+str(a.sdk/'sdk/include'),'-I'+str(MACHINE),*extra,'-I'+str(MACHINE/'vendor'),*units,'-o',exe],name+'-build')
        outputs[name]=run([exe,a.out/(name+'.bin')],name+'-run')
    reference=(a.out/'reference.bin').read_bytes();candidate=(a.out/'candidate.bin').read_bytes()
    require(len(reference)==47*6*128*64*4,'Incomplete model corpus')
    require(len(candidate)==len(reference),'Incomplete candidate corpus')
    equal=reference==candidate
    first=None
    if not equal:
        first=next(i for i,(x,y) in enumerate(zip(reference,candidate)) if x!=y)//4
    report=dict(passed=equal,models=47,scenarios=6,blocks_per_case=128,samples=len(reference)//4,
        audio_identical=equal,first_differing_sample=first,
        sources={str(s.relative_to(MACHINE)):sha(s) for s in MACHINE.rglob('*') if s.is_file()},
        reference_sha256=sha(a.out/'reference.bin'),candidate_sha256=sha(a.out/'candidate.bin'),
        reference_engine_sha256=sha(a.reference_engine),
        hardware_cpu_measured=False,limitations=['Synthetic source-level corpus; not every preset/control sequence','Host timings are not Blackfin cycle estimates','Stock XY envelopes and mixing excluded'])
    (a.out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    require(equal,'Model audio differs; inspect report')
    print(json.dumps({k:report[k] for k in ('passed','models','samples','audio_identical')}))
if __name__=='__main__':main()
