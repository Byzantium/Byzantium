
import copy
import config

def test():
	print('# Test: import config module')
	conf = config.Config('example-config.json')
	print('result: True')
	print('# Test: set() with single value')
	set_one = {'test':'123'}
	# set single value
	print('info: conf.get() before: %s' % str(conf.get()))
	set_one_ret_val = conf.set(**set_one)
	print('info: conf.get() after: %s' % str(conf.get()))
	print('set %s, returned: %s' % (str(set_one), str(set_one_ret_val)))
	set_one_key = set_one.keys()[0]
	set_one_val = set_one.values()[0]
	print('result: set_one == conf.get(set_one.keys()[0]): %s' % (str(set_one_val == conf.get(set_one_key))))
	print('# Test: get() with single value')
	# get single value
	get_ret_val = conf.get(*(set_one.keys()))
	print('info: get_ret_val %s' % str(get_ret_val))
	print('info: get value of %s' % set_one.keys()[0])
	print('result: value of %s == %s: %s' % (set_one.keys()[0], set_one.values()[0], str(get_ret_val == set_one.values()[0])))
	print('# Test: save() and load()')
	# save
	before = copy.deepcopy(conf.get())
	save_ret_val = conf.save()
	load_ret_val = conf.load()
	after = conf.get()
	print('result: save successfull: %s' % str(before == after))
	print('# Test: set() and get() with multiple values')
	# set multiple values
	multi_dict = {'test':'321', 'test0':'123', 'test1':'hello', 'test2':'world'}
	set_multi_ret_val = conf.set(**multi_dict)
	print('info: set multiple values: %s\nreturn == True: %s' % (str(set_multi_ret_val), str(multi_dict)))
	# get multiple values
	get_multi_ret_val = conf.get(*(multi_dict.keys()))
	print('info: get multiple values: %s' % str(multi_dict.keys()))
	print('result: output == input: %s' % str(get_multi_ret_val == multi_dict))
	# sync with source file with before and after printouts
	print('# Test: sync()')
	before = copy.deepcopy(conf.get())
	ret_val = conf.sync()
	after = copy.deepcopy(conf.get())
	expected_after = copy.deepcopy(before)
	print('info: before: %s' % str(before))
	print('info: after: %s' % str(after))
	print('info: ret_val: %s' % str(ret_val))
	print('result: expected == after: %s' % str(expected_after == after))
	# remove value (fake)
	removed = 'test'
	before = copy.deepcopy(conf.get())
	ret_val = conf.remove(removed)
	after = copy.deepcopy(conf.get())
	del before[removed]
	print('(before - removed) == after: %s' % str((before == after)))
	# load source to self._config
	ret_val = conf.load()
	print('conf.load() returned: %s' % str(ret_val))

if __name__ == '__main__':
	test()

