# -*- coding: utf-8 -*-
# vim: set expandtab tabstop=4 shiftwidth=4 :

import copy
import config

'''
    test(): A method that implements unit tests run on the Model() metaclass.
'''
def test():
    # Can the configuration module be successfully imported and instantiated?
    print('# Test: import config module')
    conf = config.Config('example-config.json')
    print('result: True')
    print

    # Set a single configuration value.
    print('# Test: set() with single value')
    set_one = {'test':'123'}
    print('info: conf.get() before: %s' % str(conf.get()))
    set_one_ret_val = conf.set(**set_one)

    print('info: conf.get() after: %s' % str(conf.get()))
    print('set %s, returned: %s' % (str(set_one), str(set_one_ret_val)))

    set_one_key = set_one.keys()[0]
    set_one_val = set_one.values()[0]
    print('result: set_one == conf.get(set_one.keys()[0]): %s' % (str(set_one_val == conf.get(set_one_key))))
    print

    # Get a single configuration value.
    print('# Test: get() with single value')
    get_ret_val = conf.get(*(set_one.keys()))
    print('info: get_ret_val %s' % str(get_ret_val))
    print('info: get value of %s' % set_one.keys()[0])
    print('result: value of %s == %s: %s' % (set_one.keys()[0], set_one.values()[0], str(get_ret_val == set_one.values()[0])))
    print

    # Save and load values to the test datastore to make sure it's working
    # properly.
    print('# Test: save() and load()')

    # Make an actual duplicate of the configuration data structure rather than
    # just another reference to it.
    before = copy.deepcopy(conf.get())
    save_ret_val = conf.save()
    load_ret_val = conf.load()
    after = conf.get()
    print('result: save successfull: %s' % str(before == after))
    print

    # Store multiple configuration values in a JSON string.  Dictionary.
    # Whatever.
    print('# Test: set() and get() with multiple values')
    multi_dict = {'test':'321', 'test0':'123', 'test1':'hello', 'test2':'world'}
    set_multi_ret_val = conf.set(**multi_dict)
    print('info: set multiple values: %s\nreturn == True: %s' % (str(set_multi_ret_val), str(multi_dict)))
    print

    # Retrieve those values as a dictionary.
    print('# Test: Retrieve multiple values from datastore.')
    get_multi_ret_val = conf.get(*(multi_dict.keys()))
    print('info: get multiple values: %s' % str(multi_dict.keys()))
    print('result: output == input: %s' % str(get_multi_ret_val == multi_dict))
    print

    # Synch with data source with before and after printouts to make sure that
    # what gets saved is what gets loaded.
    print('# Test: Is what gets saved also what gets loaded?')
    before = copy.deepcopy(conf.get())
    ret_val = conf.sync()
    after = copy.deepcopy(conf.get())
    expected_after = copy.deepcopy(before)
    print('info: before: %s' % str(before))
    print('info: after: %s' % str(after))
    print('info: ret_val: %s' % str(ret_val))
    print('result: expected == after: %s' % str(expected_after == after))
    print

    # Test the deletion of configuration values from the datastore.
    print('# Test: Deletion of configuration values from datastore.')
    removed = 'test'
    before = copy.deepcopy(conf.get())
    ret_val = conf.remove(removed)
    after = copy.deepcopy(conf.get())
    del before[removed]
    print('(before - removed) == after: %s' % str((before == after)))
    print

    # Test loading configuration settings from a canonical data source to load
    # the configuration model object.
    print('Test: Load configuration settings from datastore and configure model object.')
    ret_val = conf.load()
    print('conf.load() returned: %s' % str(ret_val))
    print

# Run the tests.
if __name__ == '__main__':
    test()

