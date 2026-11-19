"""Offline tests of command allowlist, identity gates and package validation.
All device identities/backup files in this test are synthetic fixtures.
"""
import contextlib,io,json,struct,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
import discovery_programmer as d

class Guards(unittest.TestCase):
    def setUp(self):
        directory=d.ROOT/'build/discovery/tool-test-fixtures';directory.mkdir(parents=True,exist_ok=True)
        self.temp=tempfile.TemporaryDirectory(dir=directory);self.root=Path(self.temp.name).resolve()
        self.conn=['-c','port=SWD','sn=066DFF575377524867063531','mode=HOTPLUG','freq=1000']
    def tearDown(self):
        if not self.root.is_relative_to((d.ROOT/'build/discovery/tool-test-fixtures').resolve()):raise ValueError('Unsafe fixture cleanup')
        self.temp.cleanup()
    def test_list_only(self):d.allow_readonly(['-l','stlink'],self.root)
    def test_only_exact_read_ranges(self):
        for address,size in d.READS:d.allow_readonly(self.conn+['--upload',hex(address),str(size),str(self.root/f'{address}.bin')],self.root)
        for address,size in [(0x08000000,512),(0x08000001,1048576),(0x40000000,4)]:
            with self.assertRaises(ValueError):d.allow_readonly(self.conn+['--upload',hex(address),str(size),str(self.root/'x.bin')],self.root)
    def test_options_display_only(self):
        d.allow_readonly(self.conn+['-ob','displ'],self.root)
        for suffix in [['-ob','RDP=0xAA'],['-e','all'],['-w','x.bin','0x08000000'],['-rdu'],['-rst'],['-halt'],['-start']]:
            with self.assertRaises(ValueError):d.allow_readonly(self.conn+suffix,self.root)
    def test_no_reset_connection(self):
        for mode in ['mode=UR','mode=NORMAL','mode=HOTPLUG reset=HWrst']:
            conn=self.conn[:];conn[3]=mode
            with self.assertRaises(ValueError):d.allow_readonly(conn+['-ob','displ'],self.root)
    def test_serial_required(self):
        for serial in ['', 'sn=auto','sn=066DFF575377524867063531 -e all']:
            conn=self.conn[:];conn[2]=serial
            with self.assertRaises(ValueError):d.allow_readonly(conn+['-ob','displ'],self.root)
    def test_no_overwrite_or_path_escape(self):
        p=self.root/'old.bin';p.write_bytes(b'original')
        for out in [p,self.root.parent/'outside.bin',d.ROOT.parent/'outside.bin']:
            with self.assertRaises(ValueError):d.allow_readonly(self.conn+['--upload','0x08000000','1048576',str(out)],self.root)
        self.assertEqual(p.read_bytes(),b'original')
    def test_identity_valid_fixture(self):
        x=d.decode_identity(struct.pack('<I',0x10016413),struct.pack('<H',1024),struct.pack('<I',0x0fffaaed))
        self.assertEqual((x['device_id'],x['flash_kib'],x['rdp'],x['nwrp']),('0x413',1024,0xaa,0xfff))
    def test_identity_rejects_ve_wrong_family_and_protection(self):
        for dev,size,rdp in [(0x413,512,0xaa),(0x419,1024,0xaa),(0x413,1024,0),(0x413,1024,0xcc)]:
            with self.assertRaises(ValueError):d.decode_identity(struct.pack('<I',dev),struct.pack('<H',size),struct.pack('<I',rdp<<8))
    def test_short_identity_rejected(self):
        with self.assertRaises(ValueError):d.decode_identity(b'',b'',b'')
    def test_backup_missing_cli_never_calls_device(self):
        with patch.object(d,'locate_cli',return_value=None),patch.object(d,'pnp') as pnp,patch.object(d,'cube_read') as cube:
            with self.assertRaises(ValueError):d.backup('066DFF575377524867063531')
            pnp.assert_not_called();cube.assert_not_called()
    def test_backup_driver_error_never_calls_device(self):
        rows=[dict(InstanceId='USB\\VID_0483&PID_374B\\066DFF575377524867063531',Status='OK'),dict(InstanceId='USB\\VID_0483&PID_374B&MI_00\\x',Status='Error')]
        with patch.object(d,'locate_cli',return_value=self.root/'fake.exe'),patch.object(d,'pnp',return_value=rows),patch.object(d,'cube_read') as cube:
            with self.assertRaises(ValueError):d.backup('066DFF575377524867063531')
            cube.assert_not_called()
    def fixture_package(self):
        package=self.root/'package';(package/'stm32').mkdir(parents=True)
        binary=bytearray(512);struct.pack_into('<II',binary,0,0x20020000,0x08000189);binary[400:400+len(d.TAG)]=d.TAG
        image=package/'stm32/tyvor_discovery_bench.bin';image.write_bytes(binary)
        manifest=dict(board='STM32F4 Discovery MB997E',mcu='STM32F407VGT6',flash_address='0x08000000',uart_gate='OFF',rf_outputs=False,
                      files=[dict(path='stm32/tyvor_discovery_bench.bin',size=len(binary),sha256=d.sha(image))])
        self.manifest=package/'SHA256_MANIFEST.json';self.manifest.write_text(json.dumps(manifest));return package,manifest
    def test_synthetic_package_passes_offline_only(self):
        p,_=self.fixture_package();r=d.preflight(p);self.assertFalse(r['flashed']);self.assertFalse(r['hardware_ready'])
    def test_wrong_board_rejected(self):
        p,m=self.fixture_package();m['mcu']='STM32F407VET6';self.manifest.write_text(json.dumps(m))
        with self.assertRaises(ValueError):d.preflight(p)
    def test_tamper_and_extra_file_rejected(self):
        p,m=self.fixture_package();binary=p/'stm32/tyvor_discovery_bench.bin';original=binary.read_bytes();binary.write_bytes(original+b'x')
        with self.assertRaises(ValueError):d.preflight(p)
        binary.write_bytes(original);(p/'unexpected.bin').write_bytes(b'x')
        with self.assertRaises(ValueError):d.preflight(p)
    def test_manifest_path_escape_rejected(self):
        p,m=self.fixture_package();m['files'][0]['path']='../outside.bin';self.manifest.write_text(json.dumps(m))
        with self.assertRaises(ValueError):d.preflight(p)
    def test_flash_plan_missing_backup_no_subprocess(self):
        p,_=self.fixture_package();b=self.root/'backup.json';r=self.root/'review.json';b.write_text('{"backup_verified":false}');r.write_text('{}')
        with patch.object(d.subprocess,'run') as run:
            with self.assertRaises(ValueError):d.flash_plan(p,b,r,'066DFF575377524867063531')
            run.assert_not_called()
    def test_valid_synthetic_flash_plan_is_print_only(self):
        p,_=self.fixture_package();b=self.root/'backup-record.json';r=self.root/'review.json'
        files={'device-id.bin':struct.pack('<I',0x10016413),'flash-size.bin':struct.pack('<H',1024),
               'options-before.bin':struct.pack('<I',0x0fffaaed),'options-after.bin':struct.pack('<I',0x0fffaaed),'original.bin':bytes(1048576)}
        for name,data in files.items():(self.root/name).write_bytes(data)
        record=d.decode_identity(files['device-id.bin'],files['flash-size.bin'],files['options-before.bin'])
        record.update(backup_verified=True,probe_serial='066DFF575377524867063531',backup_path=(self.root/'original.bin').relative_to(d.ROOT).as_posix(),backup_sha256=d.sha(self.root/'original.bin'),
                      evidence_files=[dict(path=(self.root/name).relative_to(d.ROOT).as_posix(),size=len(data),sha256=d.sha(self.root/name)) for name,data in files.items()])
        b.write_text(json.dumps(record));review=dict(board_marking='MB997E',mcu_marking='STM32F407VGT6',reviewer='SYNTHETIC TEST ONLY',date='fixture')
        review.update({key:True for key in ['jumpers_verified','power_verified','hse_mco_8mhz_verified','overwrite_original_approved','recovery_plan_reviewed']});r.write_text(json.dumps(review))
        with patch.object(d.subprocess,'run') as run,patch.object(d,'locate_cli',return_value=self.root/'fake-cli.exe'):
            result=d.flash_plan(p,b,r,'066DFF575377524867063531')
            self.assertEqual(result['status'],'PRINTED_PLAN_ONLY');self.assertFalse(result['flashed']);self.assertIn('-w',result['command']);run.assert_not_called()
            (self.root/'options-before.bin').write_bytes(bytes(4))
            with self.assertRaises(ValueError):d.flash_plan(p,b,r,'066DFF575377524867063531')
            run.assert_not_called()

if __name__=='__main__':
    stream=io.StringIO();result=unittest.TextTestRunner(stream=stream,verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Guards))
    out=d.EVIDENCE;out.mkdir(parents=True,exist_ok=True);(out/'discovery-tools-tests.txt').write_text(stream.getvalue())
    (out/'discovery-tools-tests.json').write_text(json.dumps(dict(tests=result.testsRun,failed=len(result.failures),errors=len(result.errors),skipped=len(result.skipped),hardware_tested=False,fixtures_only=True),indent=2)+'\n')
    print(stream.getvalue());raise SystemExit(0 if result.wasSuccessful() else 1)
