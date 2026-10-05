"""Run in `pebble repl --emulator aplite` after a fresh app install:
exec(open('tests/watch_smoke.py').read())
Uses a fake companion; never connects to an Android device or places a call.
"""
import queue
import subprocess
import time
from pathlib import Path
from uuid import UUID
from libpebble2.services.appmessage import AppMessageService, Int32, CString

app_uuid = UUID('8269f312-4d4c-4149-95fa-9f5042fcf467')
messages = queue.Queue()
service = AppMessageService(pebble)
service.register_handler('appmessage', lambda tid, uuid, data: messages.put(data) if uuid == app_uuid else None)


def click(button, count=1):
    subprocess.run(['pebble', 'emu-button', '--emulator', 'aplite', 'click', button,
                    '--repeat', str(count), '--interval', '100'], check=True)
    time.sleep(0.15)


def receive(request):
    data = messages.get(timeout=5)
    assert data[1] == request, data
    return data


def reply(data, result=0, entries=None, offset=0, total=21, final=True):
    response = {2: Int32(data[2]), 9: Int32(result)}
    if entries is not None:
        response.update({10: Int32(total), 5: Int32(offset), 11: CString(entries)})
    if final:
        response[12] = Int32(1)
    service.send_message(app_uuid, response)
    time.sleep(0.2)


def screenshot(name):
    Path('build/qa').mkdir(parents=True, exist_ok=True)
    subprocess.run(['pebble', 'screenshot', '--emulator', 'aplite', '--no-open',
                    'build/qa/' + name + '.png'], check=True)


try:
    click('select')  # Dialer
    click('select')  # 1
    click('down')
    click('select')  # 2
    click('down')
    click('select')  # 3
    click('up', 3)   # Call
    click('select')  # confirmation
    assert messages.empty(), 'Calling must require confirmation'
    click('select')
    call = receive(3)
    assert call[13] == '123', call
    reply(call, result=1)
    click('select', 2)
    assert messages.empty(), 'Repeated Select must not place duplicate calls'
    click('back')
    click('back')
    click('down')    # Recent calls
    click('select')
    page = receive(1)
    assert page[3] == 3 and page[5] == 0 and page[6] == 20, page
    for offset in (0, 10):
        entries = '\x1e'.join(f'{i + 1}\x1fPerson {i + 1}\x1fMissed - Oct 5 10:00'
                              for i in range(offset, offset + 10))
        reply(page, entries=entries, offset=offset, final=offset == 10)
    screenshot('aplite-recents')
    click('down', 20)  # More
    click('select')
    page = receive(1)
    assert page[5] == 20, page
    reply(page, entries='9223372036854775807\x1fLast caller\x1fOutgoing - Oct 5 09:00', offset=20)
    click('select')
    click('select')
    call = receive(2)
    assert call[3] == 3 and call[8] == '9223372036854775807', call
    reply(call, result=1)
    click('back')
    click('up')      # Previous
    click('select')
    page = receive(1)
    assert page[5] == 0, page
    reply(page, result=4)
    screenshot('aplite-call-history-permission')
    print('PASS: dial confirmation, number transport, duplicate suppression, recent-call batches, '
          'next/previous paging, 64-bit call ID and permission response')
finally:
    service.shutdown()
