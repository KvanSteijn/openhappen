<?php

namespace OpenHappen\Bot;

use OpenHappen\DataProvider;

/* Data providers */
require_once __DIR__ . '/../DataProvider/JSON.php.inc';

/* Bot scripts */
require_once __DIR__ . '/Href.php.inc';
require_once __DIR__ . '/SmartDOM.php.inc';
require_once __DIR__ . '/Request.php.inc';
require_once __DIR__ . '/Sitemap.php.inc';
require_once __DIR__ . '/Robots.php.inc';
require_once __DIR__ . '/Page.php.inc';

class Bot {

    const TYPE_PAGE = 'page';
    const TYPE_SITEMAP = 'sitemap';

    protected $_version = 0;

    protected $_dataProvider = NULL;
    protected $_domainExtensionCheck = FALSE;
    protected $_domainExtensions = [];

    public function __construct($dataProvider, array $domainExtensions = []) {
        $this->_dataProvider = $dataProvider;
        $this->_domainExtensionCheck = !empty($domainExtensions);
        $this->_domainExtensions = $domainExtensions;

    }

    protected function _log(string $text) {
        echo '[' . date('r') . '] ' . $text . PHP_EOL;
    }

    protected function _checkHrefs(string $type,  Request $request, array $hrefs) {
        /* Get URL's */
        $urls = [];
        foreach ($hrefs as $href) {
            $url = $href->getURL($request->getDomainURL());
            switch ($type) {
                case self::TYPE_PAGE:
                    if ($this->_dataProvider->retrievePage($url)) {
                        $urls[] = $url;
                    }
                    break;
                case self::TYPE_SITEMAP:
                    $urls[] = $url;
                    break;
            }
        }

        /* Return URL's */
        return $urls;
    }

    public function init() : array {
        /* Init data provider */
        $result = $this->_dataProvider->init();
        list($status, $message) = $result;
        if (!$status) {
            $this->_dataProvider = NULL;
        }

        /* Return result */
        return $result;
    }

    public function start($url) {
        do {
            /* Check if url value is not empty */
            if (!empty($url)) {
                list($page, $message) = $this->progressPage($url, TRUE);
                if ($page !== NULL) {
                    /* Loop sitemaps */
                    $robots = $page->getRobots();
                    $sitemapHrefs = $robots->getSitemapHrefs();
                    $urls = $this->_checkHrefs(self::TYPE_SITEMAP, $page->getRequest(), $sitemapHrefs);
                    foreach ($urls as $url) {
                        list($status, $message) = $this->progressSitemap($url, $robots);
                        if (!$status) {
                            $this->_log($message);
                        }
                    }
                } else {
                    $this->_log($message);
                }
                $url = NULL;
            }
        } while (empty($url));
    }

    public function progressSitemap(string $url, Robots $robots) : array {
        /* Create log log */
        $this->_log('Processing sitemap ' . $url);

        /* Create page object */
        $sitemap = new Sitemap($url);

        /* Check if crawl delay is higher than zero */
        $crawlDelay = $robots->getCrawlDelay();
        if ($crawlDelay > 0) {
            $this->_log('Crawl-delay found. Sleep for ' . $crawlDelay . ' seconds');
            sleep($crawlDelay);
        }

        /* Retrieve page */
        list($status, $message) = $sitemap->retrieve();
        if (!$status) {
            return [NULL, 'Failed to retrieve sitemap: ' . $message];
        }

        /* Add sitemap */
        $this->_dataProvider->addSitemap($sitemap);

        /* Get sitemap hrefs */
        $sitemapHrefs = $sitemap->getSitemapHrefs();
        $urls = $this->_checkHrefs(self::TYPE_SITEMAP, $sitemap->getRequest(), $sitemapHrefs);
        foreach ($urls as $url) {
            list($status, $message) = $this->progressSitemap($url, $robots);
            if (!$status) {
                $this->_log($message);
            }
        }

        /* Success */
        return [TRUE, ''];
    }

    public function progressPage(string $url, bool $deep = FALSE, Robots $robots = NULL) : array {
        /* Create log log */
        $this->_log('Processing page ' . $url);

        /* Check if data provider is NULL */
        if ($this->_dataProvider === NULL) {
            return [NULL, 'Data provider is not valid'];
        }

        /* Create page object */
        $page = new Page($url, $robots);

        /* Get request */
        $request = $page->getRequest();

        /* Check if domain extension check is enabled */
        if ($this->_domainExtensionCheck && !in_array($request->getExtension(), $this->_domainExtensions)) {
            return [NULL, 'Extension is different than the required extensions'];
        }

        /* Init page */
        list($status, $message) = $page->init();
        if (!$status) {
            return [NULL, 'Failed to init page: ' . $message];
        }

        /* Get robots object */
        $robots = $page->getRobots();

        /* Check if crawl delay is higher than zero */
        $crawlDelay = $robots->getCrawlDelay();
        if ($crawlDelay > 0) {
            $this->_log('Crawl-delay found. Sleep for ' . $crawlDelay . ' seconds');
            sleep($crawlDelay);
        }

        /* Retrieve page */
        list($status, $message) = $page->retrieve();
        if (!$status) {
            return [NULL, 'Failed to retrieve page: ' . $message];
        }

        /* Add page */
        $this->_dataProvider->addPage($page);

        /* Check if deep is TRUE */
        if ($deep) {
            /* Create empty array for websites */
            $internalPages = [];

            /* Check if internal hrefs exists */
            $internalHrefs = $page->getInternalHrefs();
            $urls = $this->_checkHrefs(self::TYPE_PAGE, $request, $internalHrefs);
            foreach ($urls as $url) {
                list($childPage, $message) = $this->progressPage($url, FALSE, $robots);
                if ($childPage === NULL) {
                    $this->_log($message);
                } else {
                    $internalPages[] = $childPage;
                }
            }

            /* Run deeper */
            foreach ($internalPages as $internalPage) {
                $internalHrefs = $internalPage->getInternalHrefs();
                $urls = $this->_checkHrefs(self::TYPE_PAGE, $internalPage->getRequest(), $internalHrefs);
                foreach ($urls as $url) {
                    list($childPage, $message) = $this->progressPage($url, TRUE, $robots);
                    if ($childPage === NULL) {
                        $this->_log($message);
                    }
                }
            }
        }

        /* Success */
        return [$page, ''];
    }
}

/* Check if you are running this script in CLI mode */
if (php_sapi_name() !== 'cli') {
    exit('Run only this script in CLI mode' . PHP_EOL);
}

/* Set some default info */
$urlValue = NULL;
$dataProviderValue = 'json';
$domainExtensionValues = [];

/* Get url path */
$urlKey = array_search('--url', $argv);
if ($urlKey !== FALSE && !empty($argv[$urlKey + 1])) {
    $urlValue = $argv[$urlKey + 1];
}

/* Get data provider */
$dataProviderKey = array_search('--data-provider', $argv);
if ($dataProviderKey !== FALSE && !empty($argv[$dataProviderKey + 1])) {
    $dataProviderValue = $argv[$dataProviderKey + 1];
}

/* Get domain extenions */
$domainExtensionsKey = array_search('--only-domain-extensions', $argv);
if ($domainExtensionsKey !== FALSE && !empty($argv[$domainExtensionsKey + 1])) {
    $domainExtensionValues = explode(',', $argv[$domainExtensionsKey + 1]);
}

/* Get data provider object */
switch ($dataProviderValue) {
    case 'json':
        $dataProviderObj = new DataProvider\JSON;
        break;
    default:
        exit('Unspported data provider' . PHP_EOL);
}

/* Create bot object */
$bot = new Bot($dataProviderObj, $domainExtensionValues);

/* Init bot */
list($status, $message) = $bot->init();
if (!$status) {
    exit($message . PHP_EOL);
}

/* Start bot */
$bot->start($urlValue);
