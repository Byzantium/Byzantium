
import config

def test():
	conf = config.Config('example-config.json')
	# set single value
	ret_val = conf.set(test='123')
	print(ret_val)
	# get single value
	ret_val = conf.get('test')
	print(ret_val)
	# save
	ret_val = conf.save()
	print(ret_val)
	# set multiple values
	ret_val = conf.set(test='321', test0='123', test1='hello', test2='world')
	print(ret_val, {'test':'321', 'test0':'123', 'test1':'hello', 'test2':'world'})
	# get multiple values
	ret_val = conf.get('test', 'test0', 'test1', 'test2')
	print(ret_val)
	# sync with source file with before and after printouts
	before = conf.get()
	ret_val = conf.sync()
	after = conf.get()
	print('before: %s' % str(before))
	print('after: %s' % str(after))
	print('ret_val: %s' % str(ret_val))
	# remove value (fake)
	before = conf.get()
	ret_val = conf.remove('test')
	after = conf.get()
	print('before: %s' % str(before))
	print('after: %s' % str(after))
	print('ret_val: %s' % str(ret_val))
	# load source to self._config
	ret_val = conf.load()
	print(ret_val)

if __name__ == '__main__':
	test()
	
