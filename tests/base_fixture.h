/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////////////
/// @file
/// @brief IronBee --- Base fixture for Ironbee tests
///
/// @author Craig Forbes <cforbes@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#ifndef __BASE_FIXTURE_H__
#define __BASE_FIXTURE_H__

#include <ironbee/release.h>
#include <ironbee/core.h>
#include <ironbee/state_notify.h>
#include <ironbee/util.h>

#include "gtest/gtest.h"

#include <stdexcept>
#include <string>

#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>

#define ASSERT_IB_OK(x) ASSERT_EQ(IB_OK, (x))
static const size_t EXCEPTION_BUF_SIZE = 128;

class BaseFixture : public ::testing::Test {
public:
    virtual void SetUp() {
        ib_status_t rc;
        char buf[EXCEPTION_BUF_SIZE+1];

        /* Setup IronBee server object. */
        ibt_ibserver.vernum = IB_VERNUM;
        ibt_ibserver.abinum = IB_ABINUM;
        ibt_ibserver.version = IB_VERSION;
        ibt_ibserver.filename = __FILE__;
        ibt_ibserver.name = "unit_tests";

        /* Call global initialization routine. */
        rc = ib_initialize();
        if (rc != IB_OK) {
            snprintf(buf, EXCEPTION_BUF_SIZE,
                     "Failed to initialize IronBee: %s",
                     ib_status_to_string(rc));
            throw std::runtime_error(buf);
        }

        /* Create the engine. */
        rc = ib_engine_create(&ib_engine, &ibt_ibserver);
        if (rc != IB_OK) {
            snprintf(buf, EXCEPTION_BUF_SIZE,
                     "Failed to create IronBee Engine: %s",
                     ib_status_to_string(rc));
            throw std::runtime_error(buf);
        }

        /* Initialize the engine. */
        rc = ib_engine_init(ib_engine);
        if (rc != IB_OK) {
            snprintf(buf, EXCEPTION_BUF_SIZE,
                     "Failed to initialize IronBee Engine: %s",
                     ib_status_to_string(rc));
            throw std::runtime_error(buf);
        }

        /* Set/reset the rules base path and modules base path.*/
        resetRuleBasePath();
        resetModuleBasePath();
    }

    /**
     * Reset the rule base path configuration in this IronBee engine to a default for testing.
     */
    void resetRuleBasePath()
    {
        setRuleBasePath(IB_XSTRINGIFY(RULE_BASE_PATH));
    }

    /**
     * Set the rules base path in ib_engine to be @a path.
     * @param[in] path The path to the rules.
     */
    void setRuleBasePath(const char* path)
    {
        ib_core_cfg_t *corecfg = NULL;
        ib_context_module_config(ib_context_main(ib_engine),
                                 ib_core_module(),
                                 static_cast<void*>(&corecfg));
        corecfg->rule_base_path = path;
    }

    /**
     * Reset the module base path configuration in this IronBee engine to a default for testing.
     */
    void resetModuleBasePath()
    {
        setModuleBasePath(IB_XSTRINGIFY(MODULE_BASE_PATH));
    }

    /**
     * Set the module base path in ib_engine to be @a path.
     * @param[in] path The path to the modules.
     */
    void setModuleBasePath(const char* path)
    {
        ib_core_cfg_t *corecfg = NULL;
        ib_context_module_config(ib_context_main(ib_engine),
                                 ib_core_module(),
                                 static_cast<void*>(&corecfg));
        corecfg->module_base_path = path;
    }

    std::string getBasicIronBeeConfig()
    {
        return std::string(
            "# A basic ironbee configuration\n"
            "# for getting an engine up-and-running.\n"
            "LogLevel 9\n"

            "LoadModule \"ibmod_htp.so\"\n"
            "LoadModule \"ibmod_pcre.so\"\n"
            "LoadModule \"ibmod_ac.so\"\n"
            "LoadModule \"ibmod_rules.so\"\n"
            "LoadModule \"ibmod_user_agent.so\"\n"

            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"

            "# Disable audit logs\n"
            "AuditEngine Off\n"

            "Set parser \"htp\"\n"

            "<Site test-site>\n"
            "SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "Hostname somesite.com\n"
            "</Site>\n");
    }

    /**
     * Create a temporary configuration file and have IronBee read it in.
     */
    void configureIronBeeByString(const std::string& configurationText) {
        const char *fileNameTemplate = "ironbee_gtest.conf_XXXXXX";
        char *configFile;
        int fileFd;
        ssize_t writeSize;

        configFile = strdup(fileNameTemplate);

        if (!configFile) {
            throw std::runtime_error("Could not strdup tmp conf file string.");
        }

        fileFd = mkstemp(configFile);

        if (fileFd < 0) {
            free(configFile);
            throw std::runtime_error("Failed to open tmp ironbee conf file.");
        }

        writeSize = write(
            fileFd,
            configurationText.c_str(),
            configurationText.size());
        close(fileFd);
        if (writeSize < 0) {
            free(configFile);
            throw std::runtime_error("Failed to write whole config file.");
        }

        configureIronBee(std::string(configFile));

        unlink(configFile);

    }

    /**
     * Parse and load the configuration \c TestName.TestCase.config.
     *
     * The given file is sent through the IronBee configuration parser. It is
     * not expected that modules will be loaded through this interface, but
     * that they will have already been initialized using the
     * \c BaseModuleFixture class (a child of this class). The parsing of
     * the configuration file, then, is to setup to test the loaded module,
     * or other parsing.
     *
     * Realize, though, that nothing prevents the tester from using the
     * LoadModule directive in their configuration.
     */
    void configureIronBee(const std::string& configFile) {

        ib_status_t rc;
        ib_cfgparser_t *cp;

        rc = ib_cfgparser_create(&cp, ib_engine);
        if (rc != IB_OK) {
            throw std::runtime_error(
                std::string("Failed to create parser: ") +
                    boost::lexical_cast<std::string>(rc)
            );
        }

        rc = ib_engine_config_started(ib_engine, cp);
        if (rc != IB_OK) {
            throw std::runtime_error(
                std::string("Failed to start configuration: ") +
                    boost::lexical_cast<std::string>(rc)
            );
        }

        rc = ib_cfgparser_parse(cp, configFile.c_str());
        if (rc != IB_OK) {
            throw std::runtime_error(
                std::string("Failed to parse configuration file."));
        }

        rc = ib_engine_config_finished(ib_engine);
        if (rc != IB_OK) {
            throw std::runtime_error(
                std::string("Failed to start configuration: ") +
                    boost::lexical_cast<std::string>(rc)
            );
        }

        rc = ib_cfgparser_destroy(cp);
        if (rc != IB_OK) {
            throw std::runtime_error(
                std::string("Failed to destroy parser"));
        }
    }

    /**
     * Configure IronBee using the file @e testName.@e testCase.config.
     *
     * This is done by using the GTest api to get the current test name
     * and case and building the string @e testName.@e testCase.config and
     * passing that to configureIronBee(string).
     *
     * @throws std::runtime_error(std::string) if any error occurs.
     */
    void configureIronBee()
    {
        using ::testing::TestInfo;
        using ::testing::UnitTest;

        const TestInfo* const info =
            UnitTest::GetInstance()->current_test_info();

        const std::string configFile =
            std::string(info->test_case_name())+
            "."+
            std::string(info->name()) +
            ".config";

        if ( boost::filesystem::exists(boost::filesystem::path(configFile)) )
        {
            std::cout << "Using " << configFile << "." << std::endl;
            configureIronBee(configFile);
        }
        else
        {
            std::cout << "Could not open config "
                      << configFile
                      << ". Using default BasicIronBee.config."
                      << std::endl;
            configureIronBeeByString(getBasicIronBeeConfig());
        }
    }

    void sendDataIn(ib_conn_t *ib_conn, const std::string& req)
    {
        ib_conndata_t *ib_conndata;
        ib_status_t rc;

        rc = ib_conn_data_create(ib_conn, &ib_conndata, req.size());
        if (rc != IB_OK) {
            throw std::runtime_error(
                std::string("ib_conn_data_create failed"));
        }
        ib_conndata->dlen = req.size();
        memcpy(ib_conndata->data, req.data(), req.size());
        rc = ib_state_notify_conn_data_in(ib_engine, ib_conndata);
        if (rc != IB_OK) {
            throw std::runtime_error(
                std::string("ib_notify_conn_data_in failed"));
        }
    }

    void sendDataOut(ib_conn_t *ib_conn, const std::string& req)
    {
        ib_conndata_t *ib_conndata;
        ib_status_t rc;

        rc = ib_conn_data_create(ib_conn, &ib_conndata, req.size());
        if (rc != IB_OK) {
            throw std::runtime_error(
                std::string("ib_conn_data_create failed"));
        }
        ib_conndata->dlen = req.size();
        memcpy(ib_conndata->data, req.data(), req.size());
        rc = ib_state_notify_conn_data_out(ib_engine, ib_conndata);
        if (rc != IB_OK) {
            throw std::runtime_error(
                std::string("ib_notify_conn_data_in failed"));
        }
    }


    /**
     * Build an IronBee connection and call ib_state_notify_conn_opened on it.
     *
     * You should call ib_state_notify_conn_closed(ib_engine, ib_conn)
     * when done.
     *
     * The connection will be initialized with a local address of
     * 1.0.0.1:80 and a remote address of 1.0.0.2:65534.
     *
     * @returns The Initialized IronbeeConnection.
     */
    ib_conn_t* buildIronBeeConnection()
    {
        ib_conn_t* ib_conn;
        ib_conn_create(ib_engine, &ib_conn, NULL);
        ib_conn->local_ipstr = "1.0.0.1";
        ib_conn->remote_ipstr = "1.0.0.2";
        ib_conn->remote_port = 65534;
        ib_conn->local_port = 80;
        ib_state_notify_conn_opened(ib_engine, ib_conn);

        return ib_conn;
    }

    void loadModule(ib_module_t **ib_module,
                    const std::string& module_file)
    {
        ib_status_t rc;

        std::string module_path =
            std::string(IB_XSTRINGIFY(MODULE_BASE_PATH)) +
            "/" +
            module_file;

        rc = ib_module_load(ib_module, ib_engine, module_path.c_str());

        if (rc != IB_OK) {
            throw std::runtime_error("Failed to load module " + module_file);
        }

        rc = ib_module_init(*ib_module, ib_engine);

        if (rc != IB_OK) {
            throw std::runtime_error("Failed to init module " + module_file);
        }
    }

    virtual void TearDown() {
        ib_engine_destroy(ib_engine);
        ib_shutdown();
    }

    virtual ~BaseFixture(){}

    ib_engine_t *ib_engine;
    ib_server_t ibt_ibserver;
};

/**
 * Testing fixture by which to test IronBee modules.
 *
 * Users of this class should extend it and pass in the name of the module to
 * be tested.
 *
 * @code
 * class MyModTest : public BaseModuleFixture {
 *     public:
 *     MyModTest() : BaseModuleFixture("mymodule.so") { }
 * };
 *
 * TEST_F(MyModTest, test001) {
 *   // Test the module!
 * }
 * @endcode
 */
class BaseModuleFixture : public BaseFixture {
protected:
    //! The file name of the module.
    std::string m_module_file;

    //! The setup module is stored here.
    ib_module_t *ib_module;


public:
    BaseModuleFixture(const std::string& module_file) :
        m_module_file(module_file),
        ib_module(NULL)
    {}

    virtual void SetUp()
    {
        BaseFixture::SetUp();

        loadModule(&ib_module, m_module_file);
    }

    virtual void TearDown() {
        ib_status_t rc;
        rc = ib_module_unload(ib_module);

        if (rc != IB_OK) {
            std::cerr<<"Failed to unload module "
                     <<m_module_file
                     <<" with ib_status of "
                     <<rc
                     <<std::endl;
        }

        BaseFixture::TearDown();
    }

    virtual ~BaseModuleFixture(){}
};

#endif /* __BASE_FIXTURE_H__ */
